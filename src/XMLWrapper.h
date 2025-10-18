
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

using namespace std;

struct XMLReadMemory final : public runtime_error
{
	explicit XMLReadMemory(const string &message) : runtime_error(message) {};
	~XMLReadMemory() noexcept override = default;

	[[nodiscard]] virtual string_view type() const noexcept { return "XMLReadMemory"; }
};

class XMLWrapper
{

  public:
	XMLWrapper();
	~XMLWrapper();

	void loadXML(
		const string &url, long timeoutInSeconds, const string &basicAuthenticationUser, const string &basicAuthenticationPassword,
		const vector<string> &otherHeaders, int maxRetryNumber, int secondsToWaitBeforeToRetry, const vector<pair<string, string>> &nameServices
	);

	[[nodiscard]] string toString() const;
	static string nodeToString(xmlNodePtr node);

	[[nodiscard]] xmlNodePtr asRootNode() const;

	xmlXPathObjectPtr xPath(const string &xPathExpression, xmlNodePtr startingNode = nullptr, bool noErrorLog = false) const;

	static string asAttribute(xmlNodePtr node, const string &attributeName, bool emptyOnError = false);

	string asAttribute(string xPathExpression, const string& attributeName, xmlNodePtr startingNode = nullptr, bool emptyOnError = false) const;

	vector<string> asAttributesList(const string &xPathExpression, const string &attributeName, xmlNodePtr startingNode = nullptr, bool emptyOnError = false) const;

	string asText(const string &xPathExpression, xmlNodePtr startingNode, bool emptyOnError = false) const;

	bool tagExist(const string &xPathExpression, xmlNodePtr startingNode, bool emptyOnError = false) const;

	vector<string> asTextList(const string &xPathExpression, xmlNodePtr startingNode, bool emptyOnError = false) const;

	static void logAttributes(xmlNodePtr node);

	string _sourceXML;

  private:
	xmlDocPtr _doc;
	xmlXPathContextPtr _xpathCtx;

	void finish();
};
