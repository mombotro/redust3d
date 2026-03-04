/*
 *  Copyright (c) 2016-2021 Jeremy HU <jeremy-at-dust3d dot org>. All rights reserved. 
 *
 *  Permission is hereby granted, free of charge, to any person obtaining a copy
 *  of this software and associated documentation files (the "Software"), to deal
 *  in the Software without restriction, including without limitation the rights
 *  to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 *  copies of the Software, and to permit persons to whom the Software is
 *  furnished to do so, subject to the following conditions:

 *  The above copyright notice and this permission notice shall be included in all
 *  copies or substantial portions of the Software.

 *  THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 *  IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 *  FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 *  AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 *  LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 *  OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 *  SOFTWARE.
 */

#include <dust3d/base/rig_type.h>

namespace dust3d {

RigType RigTypeFromString(const char* typeString)
{
    std::string type = typeString;
    if (type == "Animal")
        return RigType::Animal;
    if (type == "Human")
        return RigType::Human;
    if (type == "Custom")
        return RigType::Custom;
    return RigType::None;
}

const char* RigTypeToString(RigType type)
{
    switch (type) {
    case RigType::Animal:
        return "Animal";
    case RigType::Human:
        return "Human";
    case RigType::Custom:
        return "Custom";
    default:
        return "None";
    }
}

std::string RigTypeToDispName(RigType type)
{
    switch (type) {
    case RigType::Animal:
        return std::string("Animal");
    case RigType::Human:
        return std::string("Human");
    case RigType::Custom:
        return std::string("Custom");
    default:
        return std::string("None");
    }
}

}
