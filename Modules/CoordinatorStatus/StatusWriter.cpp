#include "StatusWriter.h"

#include "json.hpp"

#include <iostream>

StandartTextWriter::~StandartTextWriter(){
	std::cout << std::endl;
}

void StandartTextWriter::PrintMessage(const std::string& msg)
{
	std::cout << msg;
}

void StandartTextWriter::PrintConnectedToolServer(const std::string& host, int16_t port, uint16_t runningTasks, uint16_t totalThreads)
{
	std::cout <<  host << ":" << port << " load:" << runningTasks << "/" << totalThreads << std::endl;
}

void StandartTextWriter::PrintSummary(int running, int thread, int queued)
{
	std::cout << std::endl <<"Total load:" << running << "/" << thread << ", queue: " << queued << std::endl;
}

void StandartTextWriter::PrintSessions(const std::string& started, int usedThreads)
{
	std::cout << "Started " << started << ", used: " << usedThreads << std::endl;
}
void StandartTextWriter::PrintTools(const std::set<std::string>& toolIds)
{
	std::cout << "\nAvailable tools: ";
	for (const auto & t : toolIds)
		std::cout << t << ", ";
}


struct JsonWriter::Impl
{
	nlohmann::ordered_json jsonResult;
};

JsonWriter::JsonWriter() : m_impl(new Impl) {};

JsonWriter::~JsonWriter() {
	std::cout << m_impl->jsonResult.dump(4) << std::endl;
}

void JsonWriter::PrintMessage(const std::string&)
{
	// stub
}
void JsonWriter::PrintConnectedToolServer(const std::string& host, int16_t port, uint16_t runningTasks, uint16_t totalThreads)
{
	m_impl->jsonResult["connected_tool_servers"][host]["port"] = port;
	m_impl->jsonResult["connected_tool_servers"][host]["running_tasks"] = runningTasks;
	m_impl->jsonResult["connected_tool_servers"][host]["total_threads"] = totalThreads;
}

void JsonWriter::PrintSummary(int running, int thread, int queued)
{
	m_impl->jsonResult["total_load"]["running"] = running;
	m_impl->jsonResult["total_load"]["thread"] = thread;
	m_impl->jsonResult["total_load"]["queue"] = queued;
}

void JsonWriter::PrintSessions(const std::string& started, int usedThreads)
{
	m_impl->jsonResult["Running sessions"].push_back({{"started", started},{"usedThreads",usedThreads}});
}
void JsonWriter::PrintTools(const std::set<std::string>& toolIds)
{
	m_impl->jsonResult["available_tools"] = toolIds;
}

std::unique_ptr<AbstractWriter> AbstractWriter::createWriter(OutType outType)
{
	switch(outType)
	{
		case OutType::JSON:
			return std::make_unique<JsonWriter>();
		default:
			assert(!"Unknown output type");
		case OutType::STD_TEXT:
			return std::make_unique<StandartTextWriter>();
	}

}
