//
// Copyright (C) OpenSim Ltd.
//
// This program is free software; you can redistribute it and/or
// modify it under the terms of the GNU Lesser General Public License
// as published by the Free Software Foundation; either version 2
// of the License, or (at your option) any later version.
//
// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU Lesser General Public License for more details.
//
// You should have received a copy of the GNU Lesser General Public License
// along with this program; if not, see <http://www.gnu.org/licenses/>.
//

#include "inet/visualizer/base/FilterBase.h"

namespace inet {

namespace visualizer {

inline bool isEmpty(const char *s) { return !s || !s[0]; }

FilterBase::~FilterBase()
{
    for (auto & matcher : matchers)
        delete matcher;
}

void FilterBase::setPattern(const char *pattern, bool dottedpath, bool fullstring, bool casesensitive)
{
    cStringTokenizer tokenizer(pattern);
    while (tokenizer.hasMoreTokens())
        matchers.push_back(new PatternMatcher(tokenizer.nextToken(), dottedpath, fullstring, casesensitive));
}

bool FilterBase::matches(const char *s)
{
    for (auto & matcher : matchers)
        if (matcher->matches(s))
            return true;

    return false;
}

} // namespace visualizer

} // namespace inet
