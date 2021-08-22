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

#pragma once

#include "ToolInvocation.h"

#include <TimePoint.h>
#include <CommonTypes.h>
#include <FileUtils.h>
#include <StringUtils.h>

#include <functional>

namespace Wuild {

/// Result of local process execution.
struct LocalExecutorResult {
    using Ptr = std::shared_ptr<LocalExecutorResult>;

    TimePoint       m_executionTime = 0;     //!< Time taken of actual process running
    bool            m_result        = false; //!< True if process exited with success code (e.g. 0)
    ByteArrayHolder m_outputData;            //!< Result file data
    std::string     m_stdOut;                //!< Console output of process

    LocalExecutorResult(const std::string& stdOut = std::string(), bool result = false)
        : m_result(result)
        , m_stdOut(stdOut)
    {}
};

/// Struct containing information for local tool invocation. When task finished, m_callback is called.
struct LocalExecutorTask {
    using Ptr          = std::shared_ptr<LocalExecutorTask>;
    using CallbackType = std::function<void(LocalExecutorResult::Ptr)>;

    CallbackType    m_callback;          //!< Called when process finished or on fail.
    ToolInvocation  m_invocation;        //!< Commandline arguments
    ByteArrayHolder m_inputData;         //!< Some input file data
    CompressionInfo m_compressionInput;  //!< Compression information
    CompressionInfo m_compressionOutput; //!< Compression information

    bool          m_writeInput = true;
    bool          m_readOutput = true;
    bool          m_setEnv     = true;
    bool          m_readStderr = true;
    TemporaryFile m_inputFile;  //!< Temporary file used for tool input
    TemporaryFile m_outputFile; //!< Temporary file used for tool output

    TimePoint m_executionStart = 0;

    std::string GetShortErrorInfo() const
    {
        return m_invocation.m_id.m_toolId + ": " + m_invocation.GetArgsString(false);
    }

    void ErrorResult(const std::string& text) const
    {
        m_callback(LocalExecutorResult::Ptr(new LocalExecutorResult(text)));
    }
};

}
