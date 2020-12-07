#pragma once

#include <memory>
#include <string>
#include <set>

class AbstractWriter
{
public:
	enum class OutType{
		JSON,
		STD_TEXT,
	};

	virtual ~AbstractWriter() = default;
	virtual void PrintMessage(const std::string& msg) = 0;
	virtual void PrintConnectedToolServer(const std::string& host, int16_t port, uint16_t runningTasks, uint16_t totalThreads) = 0;
	virtual void PrintSummary(int running, int thread, int queued) = 0;
	virtual void PrintSessions(const std::string& started, int usedThreads) = 0;
	virtual void PrintTools(const std::set<std::string>& toolIds) = 0;

	static std::unique_ptr<AbstractWriter> createWriter(AbstractWriter::OutType outType);
};

class StandartTextWriter: public AbstractWriter
{
public:
	~StandartTextWriter();
	void PrintMessage(const std::string& msg) override;
	void PrintConnectedToolServer(const std::string& host, int16_t port, uint16_t runningTasks, uint16_t totalThreads) override;
	void PrintSummary(int running, int thread, int queued) override;
	void PrintSessions(const std::string& started, int usedThreads) override;
	void PrintTools(const std::set<std::string>& toolIds) override;
};

class JsonWriter: public AbstractWriter
{
public:
	JsonWriter();
	~JsonWriter();
	void PrintMessage(const std::string& msg) override;
	void PrintConnectedToolServer(const std::string& host, int16_t port, uint16_t runningTasks, uint16_t totalThreads) override;
	void PrintSummary(int running, int thread, int queued) override;
	void PrintSessions(const std::string& started, int usedThreads) override;
	void PrintTools(const std::set<std::string>& toolIds) override;

private:
	struct Impl;
	std::unique_ptr<Impl> m_impl;
};
