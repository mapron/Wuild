#include "StatusWriter.h"

#include "json.hpp"

StandardTextWriter::StandardTextWriter(std::ostream& stream)
	: m_ostream(stream)
{
}

StandardTextWriter::~StandardTextWriter()
{
	m_ostream << std::endl;
}

void StandardTextWriter::FormatMessage(const std::string& msg)
{
	m_ostream << msg;
}

void StandardTextWriter::FormatConnectedToolServer(const std::string& host, int16_t port, uint16_t runningTasks, uint16_t totalThreads)
{
	m_ostream <<  host << ":" << port << " load:" << runningTasks << "/" << totalThreads << std::endl;
}

void StandardTextWriter::FormatSummary(int running, int thread, int queued)
{
	m_ostream << std::endl <<"Total load:" << running << "/" << thread << ", queue: " << queued << std::endl;
}

void StandardTextWriter::FormatSessions(const std::string& started, int usedThreads)
{
	m_ostream << "Started " << started << ", used: " << usedThreads << std::endl;
}
void StandardTextWriter::FormatOverallTools(const std::set<std::string>& toolIds)
{
	m_ostream << "\nAvailable tools: ";
	for (const auto & t : toolIds)
		m_ostream << t << ", ";
	m_ostream << "\n";
}

void StandardTextWriter::FormatToolsByToolsServer(const AbstractWriter::ToolsMap& toolsByHost)
{
	m_ostream << "\nAvailable tools by toolserver: \n";
	for (const auto & toolserver : toolsByHost) {
		m_ostream << "\t" << toolserver.first << ":\t";
		for (const auto & toolId : toolserver.second)
			m_ostream << toolId << ", ";
		m_ostream << "\n";
	}
}


struct JsonWriter::Impl
{
	nlohmann::ordered_json jsonResult;
};

JsonWriter::JsonWriter(std::ostream & stream)
	: m_impl(new Impl)
	, m_ostream(stream) {
};

JsonWriter::~JsonWriter() {
	m_ostream << m_impl->jsonResult.dump(4) << std::endl;
}

void JsonWriter::FormatMessage(const std::string&)
{
	// stub
}
void JsonWriter::FormatConnectedToolServer(const std::string& host, int16_t port, uint16_t runningTasks, uint16_t totalThreads)
{
	m_impl->jsonResult["connected_tool_servers"][host]["port"] = port;
	m_impl->jsonResult["connected_tool_servers"][host]["running_tasks"] = runningTasks;
	m_impl->jsonResult["connected_tool_servers"][host]["total_threads"] = totalThreads;
}

void JsonWriter::FormatSummary(int running, int thread, int queued)
{
	m_impl->jsonResult["total_load"]["running"] = running;
	m_impl->jsonResult["total_load"]["thread"] = thread;
	m_impl->jsonResult["total_load"]["queue"] = queued;
}

void JsonWriter::FormatSessions(const std::string& started, int usedThreads)
{
	m_impl->jsonResult["Running sessions"].push_back({{"started", started},{"usedThreads",usedThreads}});
}
void JsonWriter::FormatOverallTools(const std::set<std::string>& toolIds)
{
	m_impl->jsonResult["available_tools"] = toolIds;
}

void JsonWriter::FormatToolsByToolsServer(const AbstractWriter::ToolsMap& toolsByHost)
{
	auto & toolsByHostJson = m_impl->jsonResult["available_tools_by_host"];
	for (const auto & toolserver : toolsByHost) 
		toolsByHostJson[toolserver.first] = toolserver.second;
}

std::unique_ptr<AbstractWriter> AbstractWriter::createWriter(OutType outType, std::ostream & stream)
{
	if (outType == OutType::JSON)
		return std::make_unique<JsonWriter> (stream);
	return std::make_unique<StandardTextWriter> (stream);
}
