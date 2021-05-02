#pragma once

#include <memory>
#include <string>
#include <set>
#include <map>
#include <vector>
#include <ios>

class AbstractWriter
{
public:
	enum class OutType{
		JSON,
		STD_TEXT,
	};
	
	using ToolsMap = std::map<std::string, std::vector<std::string>>;

	virtual ~AbstractWriter() = default;
	virtual void FormatMessage(const std::string& msg) = 0;
	virtual void FormatConnectedToolServer(const std::string& host, int16_t port, uint16_t runningTasks, uint16_t totalThreads) = 0;
	virtual void FormatSummary(int running, int thread, int queued) = 0;
	virtual void FormatSessions(const std::string& started, int usedThreads) = 0;
	virtual void FormatOverallTools(const std::set<std::string>& toolIds) = 0;
	virtual void FormatToolsByToolsServer(const ToolsMap& toolsByHost) = 0;
	
	static std::unique_ptr<AbstractWriter> createWriter(AbstractWriter::OutType outType, std::ostream & stream);
};

class StandardTextWriter: public AbstractWriter
{
public:
	StandardTextWriter(std::ostream & stream);
	~StandardTextWriter();
	void FormatMessage(const std::string& msg) override;
	void FormatConnectedToolServer(const std::string& host, int16_t port, uint16_t runningTasks, uint16_t totalThreads) override;
	void FormatSummary(int running, int thread, int queued) override;
	void FormatSessions(const std::string& started, int usedThreads) override;
	void FormatOverallTools(const std::set<std::string>& toolIds) override;
	void FormatToolsByToolsServer(const ToolsMap& toolsByHost) override;

private:
	std::ostream & m_ostream;
};

class JsonWriter: public AbstractWriter
{
public:
	JsonWriter(std::ostream & ostream);
	~JsonWriter();
	void FormatMessage(const std::string& msg) override;
	void FormatConnectedToolServer(const std::string& host, int16_t port, uint16_t runningTasks, uint16_t totalThreads) override;
	void FormatSummary(int running, int thread, int queued) override;
	void FormatSessions(const std::string& started, int usedThreads) override;
	void FormatOverallTools(const std::set<std::string>& toolIds) override;
	void FormatToolsByToolsServer(const ToolsMap& toolsByHost) override;
	
private:
	struct Impl;
	std::unique_ptr<Impl> m_impl;
	std::ostream & m_ostream;
};
