/*
 * Copyright (C) 2013 Emmanuel Durand
 *
 * This file is part of Splash.
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * blobserver is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with blobserver.  If not, see <http://www.gnu.org/licenses/>.
 */

/*
 * @texture.h
 * The Texture base class
 */

#ifndef TEXTURE_H
#define TEXTURE_H

#define GLFW_NO_GLU
#define GL_GLEXT_PROTOTYPES

#include <config.h>

#include <memory>
#include <vector>
#include <GLFW/glfw3.h>
#include <glm/glm.hpp>
#include <OpenImageIO/imagebuf.h>

#include "coretypes.h"
#include "log.h"

using namespace OIIO_NAMESPACE;

namespace Splash {

class Texture : public BaseObject
{
    public:
        /**
         * Constructor
         */
        Texture();

        /**
         * Destructor
         */
        ~Texture();

        /**
         * No copy constructor, but a move one
         */
        Texture(const Texture&) = delete;
        Texture(Texture&& t)
        {
            _glTex = t._glTex;
            _spec = t._spec;
        }

        /**
         * Sets the specified buffer as the texture on the device
         */
        Texture& operator=(const ImageBuf& img);

        /**
         * Get the id of the gl texture
         */
        GLuint getTexId() const {return _glTex;}

        /**
         * Get the buffer on the host
         */
        ImageBuf getBuffer() const;

        /**
         * Get spec of the texture
         */
        ImageSpec getSpec() const {return _spec;}

        /**
         * Set the buffer size / type / internal format
         * See glTexImage2D for information about parameters
         */
        void reset(GLenum target, GLint pLevel, GLint internalFormat, GLsizei width, GLsizei height,
                   GLint border, GLenum format, GLenum type, const GLvoid* data);

    private:
        GLuint _glTex = {0};
        ImageSpec _spec;
};

typedef std::shared_ptr<Texture> TexturePtr;

} // end of namespace

#endif // TEXTURE_H