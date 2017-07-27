/*
 * Copyright (C) 2017 Smirnov Vladimir mapron1@gmail.com
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

#include "state_rewrite.h"
#include "graph.h"

#include <Syslogger.h>

struct RuleReplace
{
	const Rule* pp;
	const Rule* cc;
	std::string toolId;
	RuleReplace() = default;
	RuleReplace(const Rule* pp_, const Rule* cc_, std::string id) : pp(pp_), cc(cc_), toolId(id) {}
};

void RewriteStateRules(State *state, IRemoteExecutor * const remoteExecutor)
{
	Wuild::Syslogger() << "RewriteStateRules";
	auto stringVectorToBindings = [](const std::vector<std::string> & rule, EvalString::TokenList & tokens)
	{
		 for (const auto & str : rule)
		 {
			 if (str[0] == '$')
			 {
				 tokens.push_back(std::make_pair(str.substr(1), EvalString::SPECIAL));
				 tokens.push_back(std::make_pair(" ", EvalString::RAW));
			 }
			 else
			 {
				 tokens.push_back(std::make_pair(str + " ", EvalString::RAW));
			 }
		 }
	};

	static const std::vector<std::string> s_ignoredArgs {  "$DEFINES", "$INCLUDES", "$FLAGS" };

	const auto rules = state->bindings_.GetRules();// we must copy rules container; otherwise we stack in infinite loop.
	std::map<const Rule*,  RuleReplace> ruleReplacement;
	for (const auto & ruleIt : rules)
	{
		const std::string & ruleName = ruleIt.first;
		const Rule * rule = ruleIt.second;
		const EvalString* command = rule->GetBinding("command");
		if (!command) continue;
		std::vector<std::string> originalRule;
		for (const auto & strPair : command->parsed_)
		{
			std::string str = strPair.first;
			if (strPair.second == EvalString::SPECIAL)
			{
				str = '$' + str;
			}
			originalRule.push_back(str);
		}
		std::vector<std::string> preprocessRule, compileRule;
		std::string toolId;
		if (remoteExecutor->PreprocessCode(originalRule, s_ignoredArgs, toolId, preprocessRule, compileRule))
		{
			Rule* rulePP = rule->Clone(ruleName + "_PP");
			state->bindings_.AddRule(rulePP);
			Rule* ruleCC = rule->Clone(ruleName + "_CC");
			state->bindings_.AddRule(ruleCC);
			rulePP->toolId_ = toolId;
			ruleCC->toolId_ = toolId;
			ruleReplacement[rule] = RuleReplace(rulePP, ruleCC, toolId);

			auto & PPtokens = rulePP->GetBinding("command")->parsed_;
			auto & CCtokens = ruleCC->GetBinding("command")->parsed_;
			PPtokens.clear();
			CCtokens.clear();
			stringVectorToBindings(preprocessRule, PPtokens);
			stringVectorToBindings(compileRule, CCtokens);

			ruleCC->RemoveBinding("deps");
			ruleCC->RemoveBinding("depfile");
			auto & descTokens = rulePP->GetBinding("description")->parsed_;
			descTokens[0].first = "Preprocessing ";
		}
	}
	Wuild::Syslogger() << "RewriteStateRules...";
	const auto paths = state->paths_;
	std::set<Edge*> erasedEdges;
	for (const auto & iter : paths)
	{
		Node* node = iter.second;
		Edge* in_egde = node->in_edge();
		if (!in_egde)
			continue;
		const Rule * in_rule = &(in_egde->rule());
		auto replacementIt = ruleReplacement.find(in_rule);
		if (replacementIt != ruleReplacement.end())
		{
			RuleReplace replacement = replacementIt->second;
			const std::string objectPath = node->path();
			const std::string sourcePath = in_egde->inputs_[0]->path();
			const std::string ppPath = remoteExecutor->GetPreprocessedPath(sourcePath, objectPath);
			Node *pp_node = state->GetNode(ppPath, node->slash_bits());

			pp_node->set_buddy(in_egde->outputs_[0]);

			auto originalBindings  = in_egde->env_->GetBindings();
			bool isRemote = true;
			if (originalBindings.find("FLAGS") != originalBindings.end())
				isRemote = isRemote && remoteExecutor->CheckRemotePossibleForFlags(replacement.toolId, originalBindings["FLAGS"] );
			if (originalBindings.find("INCLUDES") != originalBindings.end())
				isRemote = isRemote && remoteExecutor->CheckRemotePossibleForFlags(replacement.toolId, originalBindings["INCLUDES"] );

			if (!isRemote)
				continue;

			Edge* edge_pp = state->AddEdge(replacement.pp);
			Edge* edge_cc = state->AddEdge(replacement.cc);

			edge_cc->is_remote_ = true; // allow remote excution of compiler.
			edge_cc->use_temporary_inputs_ = true;  // clean preprocessed files on success.

			edge_pp->implicit_deps_ = in_egde->implicit_deps_;
			edge_pp->order_only_deps_ = in_egde->order_only_deps_;
			edge_pp->inputs_ = in_egde->inputs_;
			edge_cc->outputs_ = in_egde->outputs_;
			edge_pp->implicit_outs_ = in_egde->implicit_outs_;

			for (Node *edgeInput : in_egde->inputs_)
			{
				edgeInput->RemoveOutEdge(in_egde);
				edgeInput->AddOutEdge(edge_pp);
			}
			for (Node *edgeOutput : in_egde->outputs_)
			{
				edgeOutput->set_in_edge(edge_cc);
			}
			edge_pp->outputs_.push_back(pp_node);
			edge_cc->inputs_.push_back(pp_node);
			pp_node->set_in_edge(edge_pp);
			pp_node->AddOutEdge(edge_cc);

			edge_pp->env_ = in_egde->env_;
			edge_cc->env_ = in_egde->env_->Clone();

			auto bindings  = edge_cc->env_->GetBindings();
			edge_cc->env_->AddBinding("DEFINES", "");
			edge_cc->env_->AddBinding("DEP_FILE", "");

			if (bindings.find("IN_ABS") != bindings.end())
				edge_cc->env_->AddBinding("IN_ABS", ppPath);

			if (bindings.find("FLAGS") != bindings.end())
			{
				edge_pp->env_->AddBinding("FLAGS", remoteExecutor->FilterPreprocessorFlags(replacement.toolId, bindings["FLAGS"]));
				edge_cc->env_->AddBinding("FLAGS", remoteExecutor->FilterCompilerFlags(replacement.toolId, bindings["FLAGS"]));
			}
			if (bindings.find("INCLUDES") != bindings.end())
				edge_cc->env_->AddBinding("INCLUDES", remoteExecutor->FilterCompilerFlags(replacement.toolId, bindings["INCLUDES"]));

			in_egde->outputs_.clear();
			in_egde->inputs_.clear();
			in_egde->env_ = nullptr;
			erasedEdges.insert(in_egde);
		}
	}
	vector<Edge*> newEdges;

	for (auto * edge : state->edges_)
	{
		if (erasedEdges.find(edge) == erasedEdges.end())
			newEdges.push_back(edge);
	}
	state->edges_ = newEdges;

	Wuild::Syslogger() << "/RewriteStateRules";
}
