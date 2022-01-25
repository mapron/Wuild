/*
 * Copyright (C) 2017-2021 Smirnov Vladimir mapron1@gmail.com
 * Source code licensed under the Apache License, Version 2.0 (the "License");
 * You may not use this file except in compliance with the License.
 * You may obtain a copy of the License at http://www.apache.org/licenses/LICENSE-2.0 or in file COPYING-APACHE-2.0.txt
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.h
 */

#include "UpdateFileCommandParser.h"
#include <StringUtils.h>

#include <cassert>

namespace Wuild {

void UpdateFileCommandParser::UpdateInfo()
{
    m_invocation.m_inputNameIndex  = -1;
    m_invocation.m_outputNameIndex = -1;
    m_invocation.m_type            = ToolInvocation::InvokeType::Unknown;
    if (!m_invocation.m_arglist.m_args.empty())
        m_invocation.m_inputNameIndex = m_invocation.m_outputNameIndex = m_invocation.m_arglist.m_args.size() - 1;
}

void UpdateFileCommandParser::SetInvokeType(ToolInvocation::InvokeType /*type*/)
{
    assert(!"Unsupported logic.");
}

}
