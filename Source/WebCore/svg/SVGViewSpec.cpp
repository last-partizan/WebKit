/*
 * Copyright (C) 2007, 2010 Rob Buis <buis@kde.org>
 * Copyright (C) 2018-2019 Apple Inc. All rights reserved.
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Library General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Library General Public License for more details.
 *
 * You should have received a copy of the GNU Library General Public License
 * along with this library; see the file COPYING.LIB.  If not, write to
 * the Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
 * Boston, MA 02110-1301, USA.
 */

#include "config.h"
#include "SVGViewSpec.h"

#include "Document.h"
#include "SVGElement.h"
#include "SVGFitToViewBox.h"
#include "SVGNames.h"
#include "SVGParserUtilities.h"
#include "SVGTransformList.h"
#include "SVGTransformable.h"
#include <wtf/TZoneMallocInlines.h>
#include <wtf/text/StringParsingBuffer.h>

namespace WebCore {

WTF_MAKE_TZONE_ALLOCATED_IMPL(SVGViewSpec);

SVGViewSpec::SVGViewSpec(SVGElement& contextElement)
    : SVGFitToViewBox(&contextElement, SVGPropertyAccess::ReadOnly)
    , m_contextElement(contextElement)
    , m_transform(SVGTransformList::create(&contextElement, SVGPropertyAccess::ReadOnly))
{
    static std::once_flag onceFlag;
    std::call_once(onceFlag, [] {
        PropertyRegistry::registerProperty<SVGNames::transformAttr, &SVGViewSpec::m_transform>();
    });
}

RefPtr<SVGElement> SVGViewSpec::viewTarget() const
{
    RefPtr contextElement = m_contextElement.get();
    if (!contextElement)
        return nullptr;
    return dynamicDowncast<SVGElement>(contextElement->treeScope().getElementById(m_viewTargetString));
}

Ref<SVGTransformList> SVGViewSpec::protectedTransform()
{
    return m_transform;
}

void SVGViewSpec::reset()
{
    m_viewTargetString = emptyString();
    protectedTransform()->clearItems();
    SVGFitToViewBox::reset();
    SVGZoomAndPan::reset();
}

template<typename CharacterType> static constexpr std::array<CharacterType, 7> svgViewSpec { 's', 'v', 'g', 'V', 'i', 'e', 'w' };
template<typename CharacterType> static constexpr std::array<CharacterType, 7> viewBoxSpec { 'v', 'i', 'e', 'w', 'B', 'o', 'x' };
template<typename CharacterType> static constexpr std::array<CharacterType, 19> preserveAspectRatioSpec { 'p', 'r', 'e', 's', 'e', 'r', 'v', 'e', 'A', 's', 'p', 'e', 'c', 't', 'R', 'a', 't', 'i', 'o' };
template<typename CharacterType> static constexpr std::array<CharacterType, 9> transformSpec { 't', 'r', 'a', 'n', 's', 'f', 'o', 'r', 'm' };
template<typename CharacterType> static constexpr std::array<CharacterType, 10> zoomAndPanSpec { 'z', 'o', 'o', 'm', 'A', 'n', 'd', 'P', 'a', 'n' };
template<typename CharacterType> static constexpr std::array<CharacterType, 10> viewTargetSpec  { 'v', 'i', 'e', 'w', 'T', 'a', 'r', 'g', 'e', 't' };

bool SVGViewSpec::parseViewSpec(StringView string)
{
    return readCharactersForParsing(string, [&]<typename CharacterType> (StringParsingBuffer<CharacterType> buffer) -> bool {
        if (buffer.atEnd() || !m_contextElement)
            return false;

        if (!skipCharactersExactly(buffer, std::span { svgViewSpec<CharacterType> }))
            return false;

        if (!skipExactly(buffer, '('))
            return false;

        while (buffer.hasCharactersRemaining() && *buffer != ')') {
            if (*buffer == 'v') {
                if (skipCharactersExactly(buffer, std::span { viewBoxSpec<CharacterType> })) {
                    if (!skipExactly(buffer, '('))
                        return false;
                    auto viewBox = SVGFitToViewBox::parseViewBox(buffer, false);
                    if (!viewBox)
                        return false;
                    setViewBox(WTFMove(*viewBox));
                    if (!skipExactly(buffer, ')'))
                        return false;
                } else if (skipCharactersExactly(buffer, std::span { viewTargetSpec<CharacterType> })) {
                    if (!skipExactly(buffer, '('))
                        return false;
                    auto viewTargetStart = buffer.position();
                    skipUntil(buffer, ')');
                    if (buffer.atEnd())
                        return false;
                    m_viewTargetString = String({ viewTargetStart, buffer.position() });
                    ++buffer;
                } else
                    return false;
            } else if (*buffer == 'z') {
                if (!skipCharactersExactly(buffer, std::span { zoomAndPanSpec<CharacterType> }))
                    return false;
                if (!skipExactly(buffer, '('))
                    return false;
                auto zoomAndPan = SVGZoomAndPan::parseZoomAndPan(buffer);
                if (!zoomAndPan)
                    return false;
                setZoomAndPan(*zoomAndPan);
                if (!skipExactly(buffer, ')'))
                    return false;
            } else if (*buffer == 'p') {
                if (!skipCharactersExactly(buffer, std::span { preserveAspectRatioSpec<CharacterType> }))
                    return false;
                if (!skipExactly(buffer, '('))
                    return false;
                SVGPreserveAspectRatioValue preserveAspectRatio;
                if (!preserveAspectRatio.parse(buffer, false))
                    return false;
                setPreserveAspectRatio(preserveAspectRatio);
                if (!skipExactly(buffer, ')'))
                    return false;
            } else if (*buffer == 't') {
                if (!skipCharactersExactly(buffer, std::span { transformSpec<CharacterType> }))
                    return false;
                if (!skipExactly(buffer, '('))
                    return false;
                protectedTransform()->parse(buffer);
                if (!skipExactly(buffer, ')'))
                    return false;
            } else
                return false;

            skipExactly(buffer, ';');
        }

        if (buffer.atEnd() || *buffer != ')')
            return false;

        return true;
    });
}

}
