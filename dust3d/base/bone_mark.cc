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

#include <dust3d/base/bone_mark.h>
#include <dust3d/base/color.h>

namespace dust3d {

BoneMark BoneMarkFromString(const char* markString)
{
    std::string mark = markString;
    if (mark == "Neck")
        return BoneMark::Neck;
    if (mark == "Limb")
        return BoneMark::Limb;
    if (mark == "Tail")
        return BoneMark::Tail;
    if (mark == "Joint")
        return BoneMark::Joint;
    return BoneMark::None;
}

const char* BoneMarkToString(BoneMark mark)
{
    switch (mark) {
    case BoneMark::Neck:
        return "Neck";
    case BoneMark::Limb:
        return "Limb";
    case BoneMark::Tail:
        return "Tail";
    case BoneMark::Joint:
        return "Joint";
    default:
        return "None";
    }
}

std::string BoneMarkToDispName(BoneMark mark)
{
    switch (mark) {
    case BoneMark::Neck:
        return std::string("Neck");
    case BoneMark::Limb:
        return std::string("Limb");
    case BoneMark::Tail:
        return std::string("Tail");
    case BoneMark::Joint:
        return std::string("Joint");
    default:
        return std::string("None");
    }
}

dust3d::Color BoneMarkToColor(BoneMark mark)
{
    switch (mark) {
    case BoneMark::Neck:  return dust3d::Color(0xfd / 255.0, 0x64 / 255.0, 0x61 / 255.0);
    case BoneMark::Limb:  return dust3d::Color(0x55 / 255.0, 0xad / 255.0, 0xff / 255.0);
    case BoneMark::Tail:  return dust3d::Color(0x4c / 255.0, 0xaf / 255.0, 0x50 / 255.0);
    case BoneMark::Joint: return dust3d::Color(0xff / 255.0, 0xe0 / 255.0, 0x82 / 255.0);
    default:              return dust3d::Color(0, 0, 0, 0);
    }
}

}
