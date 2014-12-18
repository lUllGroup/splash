#include "window.h"

#include "camera.h"
#include "geometry.h"
#include "gui.h"
#include "image.h"
#include "log.h"
#include "object.h"
#include "shader.h"
#include "texture.h"
#include "timer.h"

#include <functional>
#include <glm/gtc/matrix_transform.hpp>

using namespace std;
using namespace std::placeholders;

namespace Splash {

/*************/
mutex Window::_callbackMutex;
deque<pair<GLFWwindow*, vector<int>>> Window::_keys;
deque<pair<GLFWwindow*, vector<int>>> Window::_mouseBtn;
pair<GLFWwindow*, vector<double>> Window::_mousePos;
deque<pair<GLFWwindow*, vector<double>>> Window::_scroll;

/*************/
Window::Window(GlWindowPtr w)
{
    _type = "window";

    if (w.get() == nullptr)
        return;

    _window = w;
    _isInitialized = setProjectionSurface();
    if (!_isInitialized)
        SLog::log << Log::WARNING << "Window::" << __FUNCTION__ << " - Error while creating the Window" << Log::endl;
    else
        SLog::log << Log::MESSAGE << "Window::" << __FUNCTION__ << " - Window created successfully" << Log::endl;

    _viewProjectionMatrix = glm::ortho(-1.f, 1.f, -1.f, 1.f);

    setEventsCallbacks();

    registerAttributes();

    // Get the default window size and position
    glfwGetWindowPos(_window->get(), &_windowRect[0], &_windowRect[1]);
    glfwGetWindowSize(_window->get(), &_windowRect[2], &_windowRect[3]);
}

/*************/
Window::~Window()
{
    SLog::log << Log::DEBUGGING << "Window::~Window - Destructor" << Log::endl;
}

/*************/
bool Window::getKey(int key)
{
    if (glfwGetKey(_window->get(), key) == GLFW_PRESS)
        return true;
    return false;
}

/*************/
int Window::getKeys(GLFWwindow*& win, int& key, int& action, int& mods)
{
    lock_guard<mutex> lock(_callbackMutex);
    if (_keys.size() == 0)
        return 0;

    win = _keys.front().first;
    vector<int> keys = _keys.front().second;

    key = keys[0];
    action = keys[2];
    mods = keys[3];

    _keys.pop_front();

    return _keys.size() + 1;
}

/*************/
int Window::getMouseBtn(GLFWwindow*& win, int& btn, int& action, int& mods)
{
    lock_guard<mutex> lock(_callbackMutex);
    if (_mouseBtn.size() == 0)
        return 0;

    win = _mouseBtn.front().first;
    vector<int> mouse = _mouseBtn.front().second;

    btn = mouse[0];
    action = mouse[1];
    mods = mouse[2];

    _mouseBtn.pop_front();

    return _mouseBtn.size() + 1;
}

/*************/
void Window::getMousePos(GLFWwindow*& win, int& xpos, int& ypos)
{
    lock_guard<mutex> lock(_callbackMutex);
    if (_mousePos.second.size() != 2)
        return;

    win = _mousePos.first;
    xpos = (int)_mousePos.second[0];
    ypos = (int)_mousePos.second[1];
}

/*************/
int Window::getScroll(GLFWwindow*& win, double& xoffset, double& yoffset)
{
    lock_guard<mutex> lock(_callbackMutex);
    if (_scroll.size() == 0)
        return 0;

    win = _scroll.front().first;
    xoffset = _scroll.front().second[0];
    yoffset = _scroll.front().second[1];

    _scroll.pop_front();

    return _scroll.size() + 1;
}

/*************/
bool Window::linkTo(BaseObjectPtr obj)
{
    if (dynamic_pointer_cast<Texture>(obj).get() != nullptr)
    {
        TexturePtr tex = dynamic_pointer_cast<Texture>(obj);
        setTexture(tex);
        return true;
    }
    else if (dynamic_pointer_cast<Image>(obj).get() != nullptr)
    {
        TexturePtr tex = make_shared<Texture>();
        tex->setName(getName() + "_" + obj->getName() + "_tex");
        if (tex->linkTo(obj))
        {
            _root.lock()->registerObject(tex);
            return linkTo(tex);
        }
        else
            return false;
    }
    else if (dynamic_pointer_cast<Camera>(obj).get() != nullptr)
    {
        CameraPtr cam = dynamic_pointer_cast<Camera>(obj);
        for (auto& tex : cam->getTextures())
            setTexture(tex);
        return true;
    }
    else if (dynamic_pointer_cast<Gui>(obj).get() != nullptr)
    {
        GuiPtr gui = dynamic_pointer_cast<Gui>(obj);
        setTexture(gui->getTexture());
        return true;
    }

    return false;
}

/*************/
bool Window::render()
{
    if (!_window->setAsCurrentContext()) 
    	 SLog::log << Log::WARNING << "Window::" << __FUNCTION__ << " - A previous context has not been released." << Log::endl;;
    if (_srgb)
        glEnable(GL_FRAMEBUFFER_SRGB);

    int w, h;
    glfwGetWindowSize(_window->get(), &w, &h);
    glViewport(0, 0, w, h);

#ifdef DEBUG
    glGetError();
#endif
    glDrawBuffer(GL_BACK);
    glClearColor(0.0, 0.0, 0.0, 0.0);
    glClear(GL_COLOR_BUFFER_BIT);

    _screen->getShader()->setAttribute("layout", _layout);
    _screen->getShader()->setAttribute("uniform", {"_gamma", (float)_srgb, _gammaCorrection}); 
    _screen->activate();
    //_screen->setViewProjectionMatrix(_viewProjectionMatrix, glm::dmat4(1.f));
    _screen->draw();
    _screen->deactivate();

    // Resize the input textures accordingly to the window size.
    // This goes upstream to the cameras and gui
    // Textures are resized to the number of "frame" there are, according to the layout
    bool resize = true;
    for (int i = 0; i < _inTextures.size(); ++i)
    {
        int value = _layout[i].asInt();
        for (int j = i + 1; j < _inTextures.size(); ++j)
            if (_layout[j].asInt() != value)
                resize = false;
    }
    if (resize) // We don't do this if we are directly connected to a Texture (updated from an image)
        for (auto& t : _inTextures)
            t->resize(w, h);

#ifdef DEBUG
    GLenum error = glGetError();
    if (error)
        SLog::log << Log::WARNING << _type << "::" << __FUNCTION__ << " - Error while rendering the window: " << error << Log::endl;
#endif

    if (_srgb)
        glDisable(GL_FRAMEBUFFER_SRGB);

    _window->releaseContext();

#ifdef DEBUG
    return error != 0 ? true : false;
#else
    return false;
#endif
}

/*************/
void Window::swapBuffers()
{
    if (!_window->setAsCurrentContext()) 
    	 SLog::log << Log::WARNING << "Window::" << __FUNCTION__ << " - A previous context has not been released." << Log::endl;;
    glfwSwapBuffers(_window->get());
    _window->releaseContext();
}

/*************/
bool Window::switchFullscreen(int screenId)
{
    int count;
    GLFWmonitor** monitors = glfwGetMonitors(&count);
    if (screenId >= count)
        return false;

    if (_window.get() == nullptr)
        return false;

    if (screenId != -1)
        _screenId = screenId;
    else if (screenId == _screenId)
        return true;

    const GLFWvidmode* vidmode = glfwGetVideoMode(monitors[_screenId]);

    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, SPLASH_GL_CONTEXT_VERSION_MAJOR);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, SPLASH_GL_CONTEXT_VERSION_MINOR);
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);
    glfwWindowHint(GLFW_OPENGL_DEBUG_CONTEXT, SPLASH_GL_DEBUG);
    glfwWindowHint(GLFW_VISIBLE, true);
    GLFWwindow* window;
    if (glfwGetWindowMonitor(_window->get()) == NULL)
        window = glfwCreateWindow(vidmode->width, vidmode->height, ("Splash::" + _name).c_str(), monitors[_screenId], _window->getMainWindow());
    else
        window = glfwCreateWindow(vidmode->width, vidmode->height, ("Splash::" + _name).c_str(), 0, _window->getMainWindow());

    if (!window)
    {
        SLog::log << Log::WARNING << "Window::" << __FUNCTION__ << " - Unable to create new fullscreen shared window" << Log::endl;
        return false;
    }

    _window = move(make_shared<GlWindow>(window, _window->getMainWindow()));
    updateSwapInterval();

    setEventsCallbacks();

    return true;
}

/*************/
void Window::setTexture(TexturePtr tex)
{
    bool isPresent = false;
    for (auto t : _inTextures)
        if (tex == t)
            isPresent = true;

    if (isPresent)
        return;

    _inTextures.push_back(tex);
    _screen->addTexture(tex);
}

/*************/
void Window::keyCallback(GLFWwindow* win, int key, int scancode, int action, int mods)
{
    lock_guard<mutex> lock(_callbackMutex);
    vector<int> keys {key, scancode, action, mods};
    _keys.push_back(pair<GLFWwindow*, vector<int>>(win,keys));
}

/*************/
void Window::mouseBtnCallback(GLFWwindow* win, int button, int action, int mods)
{
    lock_guard<mutex> lock(_callbackMutex);
    vector<int> btn {button, action, mods};
    _mouseBtn.push_back(pair<GLFWwindow*, vector<int>>(win,btn));
}

/*************/
void Window::mousePosCallback(GLFWwindow* win, double xpos, double ypos)
{
    lock_guard<mutex> lock(_callbackMutex);
    vector<double> pos {xpos, ypos};
    _mousePos.first = win;
    _mousePos.second = move(pos);
}

/*************/
void Window::scrollCallback(GLFWwindow* win, double xoffset, double yoffset)
{
    lock_guard<mutex> lock(_callbackMutex);
    vector<double> scroll {xoffset, yoffset};
    _scroll.push_back(pair<GLFWwindow*, vector<double>>(win, scroll));
}

/*************/
void Window::setEventsCallbacks()
{
    glfwSetKeyCallback(_window->get(), Window::keyCallback);
    glfwSetMouseButtonCallback(_window->get(), Window::mouseBtnCallback);
    glfwSetCursorPosCallback(_window->get(), Window::mousePosCallback);
    glfwSetScrollCallback(_window->get(), Window::scrollCallback);
}

/*************/
bool Window::setProjectionSurface()
{
    if (!_window->setAsCurrentContext()) 
    	 SLog::log << Log::WARNING << "Window::" << __FUNCTION__ << " - A previous context has not been released." << Log::endl;;
    glfwShowWindow(_window->get());
    glfwSwapInterval(_swapInterval);

    // Setup the projection surface
#ifdef DEBUG
    glGetError();
#endif

    _screen = make_shared<Object>();
    _screen->setAttribute("fill", {"window"});
    GeometryPtr virtualScreen = make_shared<Geometry>();
    _screen->addGeometry(virtualScreen);

#ifdef DEBUG
    GLenum error = glGetError();
    if (error)
        SLog::log << Log::WARNING << __FUNCTION__ << " - Error while creating the projection surface: " << error << Log::endl;
#endif

    _window->releaseContext();

#ifdef DEBUG
    return error == 0 ? true : false;
#else
    return true;
#endif
}

/*************/
void Window::setWindowDecoration(bool hasDecoration)
{
    if (_screenId != -1)
        return;

    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, SPLASH_GL_CONTEXT_VERSION_MAJOR);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, SPLASH_GL_CONTEXT_VERSION_MINOR);
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);
    glfwWindowHint(GLFW_OPENGL_DEBUG_CONTEXT, SPLASH_GL_DEBUG);
    glfwWindowHint(GLFW_VISIBLE, true);
    glfwWindowHint(GLFW_RESIZABLE, hasDecoration);
    glfwWindowHint(GLFW_DECORATED, hasDecoration);
    GLFWwindow* window;
    window = glfwCreateWindow(_windowRect[2], _windowRect[3], ("Splash::" + _name).c_str(), 0, _window->getMainWindow());

    // Reset hints to default ones
    glfwWindowHint(GLFW_RESIZABLE, true);
    glfwWindowHint(GLFW_DECORATED, true);

    if (!window)
    {
        SLog::log << Log::WARNING << "Window::" << __FUNCTION__ << " - Unable to update window " << _name << Log::endl;
        return;
    }

    _window = move(make_shared<GlWindow>(window, _window->getMainWindow()));
    updateSwapInterval();

    setEventsCallbacks();

    return;
}

/*************/
void Window::updateSwapInterval()
{
    if (!_window->setAsCurrentContext()) 
    	 SLog::log << Log::WARNING << "Window::" << __FUNCTION__ << " - A previous context has not been released." << Log::endl;;

    glfwSwapInterval(_swapInterval);

    _window->releaseContext();
}

/*************/
void Window::updateWindowShape()
{
    glfwSetWindowPos(_window->get(), _windowRect[0], _windowRect[1]);
    glfwSetWindowSize(_window->get(), _windowRect[2], _windowRect[3]);
}

/*************/
void Window::registerAttributes()
{
    _attribFunctions["fullscreen"] = AttributeFunctor([&](Values args) {
        if (args.size() != 1)
            return false;
        switchFullscreen(args[0].asInt());
        return true;
    }, [&]() {
        return Values({_screenId});
    });

    _attribFunctions["decorated"] = AttributeFunctor([&](Values args) {
        if (args.size() != 1)
            return false;
        _withDecoration = args[0].asInt() == 0 ? false : true;
        setWindowDecoration(_withDecoration);
        updateWindowShape();
        return true;
    }, [&]() {
        return Values({(int)_withDecoration});
    });

    _attribFunctions["srgb"] = AttributeFunctor([&](Values args) {
        if (args.size() != 1)
            return false;
        if (args[0].asInt() != 0)
            _srgb = true;
        else
            _srgb = false;
        return true;
    }, [&]() {
        return Values({_srgb});
    });

    _attribFunctions["gamma"] = AttributeFunctor([&](Values args) {
        if (args.size() != 1)
            return false;
        _gammaCorrection = args[0].asFloat();
        return true;
    }, [&]() {
        return Values({_gammaCorrection});
    });

    // Attribute to configure the placement of the various texture input
    _attribFunctions["layout"] = AttributeFunctor([&](Values args) {
        if (args.size() < 1)
            return false;
        _layout = args;
        return true;
    }, [&]() {
        return _layout;
    });

    _attribFunctions["position"] = AttributeFunctor([&](Values args) {
        if (args.size() != 2)
            return false;
        _windowRect[0] = args[0].asInt();
        _windowRect[1] = args[1].asInt();
        updateWindowShape();
        return true;
    }, [&]() {
        return Values({_windowRect[0], _windowRect[1]});
    });

    _attribFunctions["size"] = AttributeFunctor([&](Values args) {
        if (args.size() != 2)
            return false;
        _windowRect[2] = args[0].asInt();
        _windowRect[3] = args[1].asInt();
        updateWindowShape();
        return true;
    }, [&]() {
        return Values({_windowRect[2], _windowRect[3]});
    });

    _attribFunctions["swapInterval"] = AttributeFunctor([&](Values args) {
        if (args.size() != 1)
            return false;
        _swapInterval = max(-1, args[0].asInt());
        updateSwapInterval();
        return true;
    });
}

} // end of namespace
