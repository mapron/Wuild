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

#include "IRemoteExecutor.h"

#include <ConfiguredApplication.h>
#include <RemoteToolClient.h>
#include <InvocationRewriter.h>
#include <ThreadUtils.h>
#include <Syslogger.h>
#include <LocalExecutor.h>
#include <VersionChecker.h>
#include <iostream>

//#define TEST_CLIENT
#ifdef TEST_CLIENT
#include <RemoteToolServer.h>
#endif

class RemoteExecutor : public IRemoteExecutor {
    Wuild::ConfiguredApplication&            m_app;
    Wuild::IInvocationRewriter::Ptr          m_invocationRewriter;
    Wuild::RemoteToolClient::Config          m_remoteToolConfig;
    std::shared_ptr<Wuild::RemoteToolClient> m_remoteService;

#ifdef TEST_CLIENT
    Wuild::ILocalExecutor::Ptr               m_localExecutor;
    std::shared_ptr<Wuild::RemoteToolServer> m_toolServer;
#endif

    bool m_remoteEnabled      = false;
    bool m_hasStart           = false;
    int  m_minimalRemoteTasks = 0;

    std::set<Edge*>    m_activeEdges;
    std::deque<Result> m_results;
    mutable std::mutex m_resultsMutex;

public:
    RemoteExecutor(Wuild::ConfiguredApplication& app);

    void   SetVerbose(bool verbose);
    double GetMaxLoadAverage() const;

    PreprocessResult PreprocessCode(const std::vector<std::string>& originalRule,
                                    const std::vector<std::string>& ignoredArgs,
                                    std::string&                    toolId,
                                    std::vector<std::string>&       preprocessRule,
                                    std::vector<std::string>&       compileRule) const override;

    bool        CheckRemotePossibleForFlags(const std::string& toolId, const std::string& flags) const override;
    std::string GetPreprocessedPath(const std::string& sourcePath, const std::string& objectPath) const override;
    std::string FilterPreprocessorFlags(const std::string& toolId, const std::string& flags) const override;

    std::string FilterCompilerFlags(const std::string& toolId, const std::string& flags) const override;
    void        RunIfNeeded(const std::vector<std::string>& toolIds, const std::shared_ptr<SubprocessSet>& subprocessSet) override;
    void        SleepSome() const override;
    int         GetMinimalRemoteTasks() const override;

    bool CanRunMore() override;

    bool StartCommand(Edge* userData, const std::string& command) override;

    /// return true if has finished result.
    bool WaitForCommand(Result* result) override;

    void            Abort() override;
    std::set<Edge*> GetActiveEdges() override;

    std::vector<std::string> GetKnownToolNames() const override;

    ~RemoteExecutor();
};
