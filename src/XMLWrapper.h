
#pragma once

#include <stdexcept>
#include <string_view>
#ifndef SPDLOG_ACTIVE_LEVEL
#define SPDLOG_ACTIVE_LEVEL SPDLOG_LEVEL_TRACE
#endif
#include "spdlog/spdlog.h"

#include <libxml/tree.h>
#include <libxml/xpath.h>
#include <vector>

// using namespace std;

struct XMLReadMemory final : public std::runtime_error
{
	explicit XMLReadMemory(const std::string &message) : runtime_error(message) {};
	~XMLReadMemory() noexcept override = default;

	[[nodiscard]] virtual std::string_view type() const noexcept { return "XMLReadMemory"; }
};

class XMLWrapper
{

  public:
	XMLWrapper();
	~XMLWrapper();

	void loadXML(
		const std::string &url, long timeoutInSeconds, const std::string &basicAuthenticationUser, const std::string &basicAuthenticationPassword,
		const std::vector<std::string> &otherHeaders, int maxRetryNumber, int secondsToWaitBeforeToRetry,
		const std::vector<std::pair<std::string, std::string>> &nameServices
	);

	[[nodiscard]] std::string toString() const;
	static std::string nodeToString(xmlNodePtr node);

	[[nodiscard]] xmlNodePtr asRootNode() const;

	xmlXPathObjectPtr xPath(const std::string &xPathExpression, xmlNodePtr startingNode = nullptr, bool noErrorLog = false) const;

	static std::string asAttribute(xmlNodePtr node, const std::string &attributeName, bool emptyOnError = false);

	std::string asAttribute(std::string xPathExpression, const std::string& attributeName, xmlNodePtr startingNode = nullptr, bool emptyOnError = false) const;

	std::vector<std::string> asAttributesList(const std::string &xPathExpression, const std::string &attributeName, xmlNodePtr startingNode = nullptr, bool emptyOnError = false) const;

	std::string asText(const std::string &xPathExpression, xmlNodePtr startingNode, bool emptyOnError = false) const;

	bool tagExist(const std::string &xPathExpression, xmlNodePtr startingNode, bool emptyOnError = false) const;

	std::vector<std::string> asTextList(const std::string &xPathExpression, xmlNodePtr startingNode, bool emptyOnError = false) const;

	static void logAttributes(xmlNodePtr node);

	std::string _sourceXML;

  private:
	xmlDocPtr _doc;
	xmlXPathContextPtr _xpathCtx;

	void finish();
};
