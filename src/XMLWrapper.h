
#ifndef XMLWrapper_h
#define XMLWrapper_h

#include <stdexcept>
#ifndef SPDLOG_ACTIVE_LEVEL
#define SPDLOG_ACTIVE_LEVEL SPDLOG_LEVEL_TRACE
#endif
#include "spdlog/spdlog.h"

#include <fstream>
#include <libxml/parser.h>
#include <libxml/tree.h>
#include <libxml/xpath.h>
#include <libxml/xpathInternals.h>
#include <vector>

using namespace std;

struct XMLReadMemory : public runtime_error
{
	XMLReadMemory(string message) : runtime_error(message) {};
	virtual string type() { return "XMLReadMemory"; }
};

class XMLWrapper
{

  public:
	XMLWrapper();
	~XMLWrapper();

	void loadXML(
		string url, long timeoutInSeconds, string basicAuthenticationUser, string basicAuthenticationPassword, vector<string> otherHeaders,
		int maxRetryNumber, int secondsToWaitBeforeToRetry, vector<pair<string, string>> nameServices
	);

	string toString();
	string nodeToString(xmlNodePtr node);

	xmlNodePtr asRootNode();

	xmlXPathObjectPtr xPath(string xPathExpression, xmlNodePtr startingNode = nullptr, bool noErrorLog = false);

	string asAttribute(xmlNodePtr node, string attributeName, bool emptyOnError = false);

	string asAttribute(string xPathExpression, string attributeName, xmlNodePtr startingNode = nullptr, bool emptyOnError = false);

	vector<string> asAttributesList(string xPathExpression, string attributeName, xmlNodePtr startingNode = nullptr, bool emptyOnError = false);

	string asText(string xPathExpression, xmlNodePtr startingNode, bool emptyOnError = false);

	bool tagExist(string xPathExpression, xmlNodePtr startingNode, bool emptyOnError = false);

	vector<string> asTextList(string xPathExpression, xmlNodePtr startingNode, bool emptyOnError = false);

	void logAttributes(xmlNodePtr node);

	string _sourceXML;

  private:
	xmlDocPtr _doc;
	xmlXPathContextPtr _xpathCtx;

	void finish();
};

#endif
