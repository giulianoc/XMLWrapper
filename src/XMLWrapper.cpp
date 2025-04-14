
#include "XMLWrapper.h"
#include "CurlWrapper.h"
#include <exception>
#include <regex>

XMLWrapper::XMLWrapper()
{
	_doc = nullptr;
	_xpathCtx = nullptr;
}

XMLWrapper::~XMLWrapper() { finish(); }

void XMLWrapper::finish()
{
	if (_xpathCtx != nullptr)
	{
		xmlXPathFreeContext(_xpathCtx);
		_xpathCtx = nullptr;
	}
	if (_doc != nullptr)
	{
		xmlFreeDoc(_doc);
		_doc = nullptr;
	}
}

void XMLWrapper::loadXML(
	string url, long timeoutInSeconds, string basicAuthenticationUser, string basicAuthenticationPassword, vector<string> otherHeaders,
	int maxRetryNumber, int secondsToWaitBeforeToRetry, vector<pair<string, string>> nameServices
)
{
	try
	{
		finish();

		_sourceXML = CurlWrapper::httpGet(
			url, timeoutInSeconds, CurlWrapper::basicAuthorization(basicAuthenticationUser, basicAuthenticationPassword), otherHeaders, "",
			maxRetryNumber, secondsToWaitBeforeToRetry
		);

		/*
		 * The document being in memory, it have no base per RFC 2396,
		 * and the "noname.xml" argument will serve as its base.
		 */
		/*
		This is an annoying failure of the libXml library. As noted by cateof, the problem is the default namespace declaration:
		xmlns="http://www.example.com/new"
		Two choices:
		(1) get rid of that declaration in your book tag or (2) give it a name, and use that name in your tags.
		*/
		string xml = _sourceXML;
		xml = regex_replace(xml, regex("xmlns="), "xmlns:mio=");
		_doc = xmlReadMemory(xml.c_str(), xml.size(), "noname.xml", "UTF-8", 0);
		// doc = xmlParseFile("/var/log/cms/dump.xml");
		if (_doc == nullptr)
		{
			string errorMessage = std::format(
				"The xmlReadMemory failed"
				", url: {}",
				url
			);
			SPDLOG_ERROR(errorMessage);

			throw XMLReadMemory(errorMessage);
		}

		/* Create xpath evaluation context */
		_xpathCtx = xmlXPathNewContext(_doc);
		if (_xpathCtx == nullptr)
		{
			string errorMessage = std::format(
				"The xmlReadMemory failed"
				", url: {}",
				url
			);
			SPDLOG_ERROR(errorMessage);

			throw runtime_error(errorMessage);
		}

		for (pair<string, string> nameService : nameServices)
			xmlXPathRegisterNs(_xpathCtx, BAD_CAST nameService.first.c_str(), BAD_CAST nameService.second.c_str());
	}
	catch (exception &e)
	{
		SPDLOG_ERROR(
			"loadData failed"
			", url: {}"
			", e.what(): {}",
			url, e.what()
		);

		finish();

		throw;
	}
}

xmlNodePtr XMLWrapper::asRootNode()
{
	try
	{
		if (_doc == nullptr)
		{
			string errorMessage = std::format("Document not initialized");
			SPDLOG_ERROR(errorMessage);

			throw runtime_error(errorMessage);
		}

		return xmlDocGetRootElement(_doc);
	}
	catch (runtime_error &e)
	{
		SPDLOG_ERROR(
			"asRootNode failed"
			", e.what(): {}",
			e.what()
		);

		throw e;
	}
	catch (exception &e)
	{
		SPDLOG_ERROR("asRootNode failed");

		throw e;
	}
}

xmlXPathObjectPtr XMLWrapper::xPath(string xPathExpression, xmlNodePtr startingNode, bool noErrorLog)
{
	xmlXPathObjectPtr resultToBeFreed = nullptr;
	try
	{
		if (_doc == nullptr)
		{
			string errorMessage = std::format("Document not initialized");
			SPDLOG_ERROR(errorMessage);

			throw runtime_error(errorMessage);
		}

		if (startingNode == nullptr)
			_xpathCtx->node = xmlDocGetRootElement(_doc);
		else
			_xpathCtx->node = startingNode;

		xmlNodePtr node = nullptr;
		{
			resultToBeFreed = xmlXPathEvalExpression(BAD_CAST xPathExpression.c_str(), _xpathCtx);
			if (!resultToBeFreed || xmlXPathNodeSetIsEmpty(resultToBeFreed->nodesetval))
			{
				string errorMessage = std::format(
					"xmlXPathEvalExpression failed"
					", xPathExpression: {}"
					", nodeDump: {}",
					xPathExpression, nodeToString(startingNode)
				);
				if (!noErrorLog)
					SPDLOG_ERROR(errorMessage);

				throw runtime_error(errorMessage);
			}

			/*
			enum xmlXPathObjectType {
				XPATH_UNDEFINED = 0
				XPATH_NODESET = 1
				XPATH_BOOLEAN = 2
				XPATH_NUMBER = 3
				XPATH_STRING = 4
				XPATH_POINT = 5
				XPATH_RANGE = 6
				XPATH_LOCATIONSET = 7
				XPATH_USERS = 8
				XPATH_XSLT_TREE = 9
			};
			enum xmlElementType {
				XML_ELEMENT_NODE = 1
				XML_ATTRIBUTE_NODE = 2
				XML_TEXT_NODE = 3
				XML_CDATA_SECTION_NODE = 4
				XML_ENTITY_REF_NODE = 5
				XML_ENTITY_NODE = 6
				XML_PI_NODE = 7
				XML_COMMENT_NODE = 8
				XML_DOCUMENT_NODE = 9
				XML_DOCUMENT_TYPE_NODE = 10
				XML_DOCUMENT_FRAG_NODE = 11
				XML_NOTATION_NODE = 12
				XML_HTML_DOCUMENT_NODE = 13
				XML_DTD_NODE = 14
				XML_ELEMENT_DECL = 15
				XML_ATTRIBUTE_DECL = 16
				XML_ENTITY_DECL = 17
				XML_NAMESPACE_DECL = 18
				XML_XINCLUDE_START = 19
				XML_XINCLUDE_END = //  XML_DOCB_DOCUMENT_NODE= 21 removed
			};
			*/
			/*
			SPDLOG_INFO("xmlXPathEvalExpression"
				", xPathExpression: {}"
				", type: {}"
				", name[0]: {}"
				", type[0]: {}"
				", content[0]: {}"
				", children[0]->content: {}"
				,
				xPathExpression, (int) resultToBeFreed->type,
				(resultToBeFreed->type == XPATH_NODESET && resultToBeFreed->nodesetval->nodeNr > 0 ?
					(char *) resultToBeFreed->nodesetval->nodeTab[0]->name : ""),
				(resultToBeFreed->type == XPATH_NODESET && resultToBeFreed->nodesetval->nodeNr > 0 ?
					(int) resultToBeFreed->nodesetval->nodeTab[0]->type : -1),
				(resultToBeFreed->type == XPATH_NODESET && resultToBeFreed->nodesetval->nodeNr > 0
					&& resultToBeFreed->nodesetval->nodeTab[0]->type == XML_TEXT_NODE ?
					(char *) resultToBeFreed->nodesetval->nodeTab[0]->content : ""),
				(resultToBeFreed->type == XPATH_NODESET && resultToBeFreed->nodesetval->nodeNr > 0
					&& resultToBeFreed->nodesetval->nodeTab[0]->type == XML_ATTRIBUTE_NODE ?
					(char *) resultToBeFreed->nodesetval->nodeTab[0]->children->content : "")
			);
			*/

			// node = resultToBeFreed->nodesetval->nodeTab[0];
		}

		// xmlXPathFreeObject(resultToBeFreed);

		return resultToBeFreed;
	}
	catch (runtime_error &e)
	{
		if (!noErrorLog)
			SPDLOG_ERROR(
				"xPath failed"
				", xPathExpression: {}"
				", e.what(): {}",
				xPathExpression, e.what()
			);

		if (resultToBeFreed != nullptr)
			xmlXPathFreeObject(resultToBeFreed);

		throw e;
	}
	catch (exception &e)
	{
		SPDLOG_ERROR(
			"xPath failed"
			", xPathExpression: {}",
			xPathExpression
		);

		if (resultToBeFreed != nullptr)
			xmlXPathFreeObject(resultToBeFreed);

		throw e;
	}
}

string XMLWrapper::asAttribute(xmlNodePtr node, string attributeName, bool emptyOnError)
{
	xmlChar *attributeValue = nullptr;
	try
	{
		attributeValue = xmlGetProp(node, BAD_CAST attributeName.c_str());
		if (attributeValue == nullptr)
		{
			string errorMessage = std::format("attribute {} not found", attributeName);

			throw runtime_error(errorMessage);
		}
		string sAttributeValue = (char *)attributeValue;
		xmlFree(attributeValue);
		attributeValue = nullptr;

		return sAttributeValue;
	}
	catch (runtime_error &e)
	{
		if (attributeValue != nullptr)
			xmlFree(attributeValue);

		if (emptyOnError)
			return "";
		else
		{
			SPDLOG_ERROR(
				"asAttribute failed"
				", node->name: {}"
				", attributeName: {}"
				", e.what(): {}",
				(node == nullptr ? "" : (char *)node->name), attributeName.c_str(), e.what()
			);

			throw e;
		}
	}
	catch (exception &e)
	{
		SPDLOG_ERROR(
			"asAttribute failed"
			", node->name: {}"
			", attributeName: {}",
			(node == nullptr ? "" : (char *)node->name), attributeName
		);

		if (attributeValue != nullptr)
			xmlFree(attributeValue);

		throw e;
	}
}

string XMLWrapper::asAttribute(string xPathExpression, string attributeName, xmlNodePtr startingNode, bool emptyOnError)
{
	xmlXPathObjectPtr resultToBeFreed = nullptr;
	try
	{
		resultToBeFreed = xPath(xPathExpression, startingNode, emptyOnError);
	}
	catch (runtime_error &e)
	{
		if (!emptyOnError)
			SPDLOG_ERROR(
				"asAttribute failed"
				", xPathExpression: {}"
				", e.what(): {}",
				xPathExpression, e.what()
			);

		if (resultToBeFreed != nullptr)
			xmlXPathFreeObject(resultToBeFreed);

		if (emptyOnError)
			return "";
		else
			throw e;
	}
	catch (exception &e)
	{
		SPDLOG_ERROR(
			"asAttribute failed"
			", xPathExpression: {}",
			xPathExpression
		);

		if (resultToBeFreed != nullptr)
			xmlXPathFreeObject(resultToBeFreed);

		throw e;
	}

	try
	{
		string attributeValue = asAttribute(resultToBeFreed->nodesetval->nodeTab[0], attributeName, emptyOnError);

		xmlXPathFreeObject(resultToBeFreed);
		resultToBeFreed = nullptr;

		return attributeValue;
	}
	catch (runtime_error &e)
	{
		if (!emptyOnError)
			SPDLOG_ERROR(
				"asAttribute failed"
				", xPathExpression: {}"
				", e.what(): {}",
				xPathExpression, e.what()
			);

		if (resultToBeFreed != nullptr)
			xmlXPathFreeObject(resultToBeFreed);

		throw e;
	}
	catch (exception &e)
	{
		SPDLOG_ERROR(
			"asAttribute failed"
			", xPathExpression: {}",
			xPathExpression
		);

		if (resultToBeFreed != nullptr)
			xmlXPathFreeObject(resultToBeFreed);

		throw e;
	}
}

vector<string> XMLWrapper::asAttributesList(string xPathExpression, string attributeName, xmlNodePtr startingNode, bool emptyOnError)
{
	xmlXPathObjectPtr resultToBeFreed = nullptr;
	try
	{
		resultToBeFreed = xPath(xPathExpression, startingNode, emptyOnError);

		vector<string> attributes;
		for (int nodeIndex = 0; nodeIndex < resultToBeFreed->nodesetval->nodeNr; nodeIndex++)
		{
			xmlNodePtr node = resultToBeFreed->nodesetval->nodeTab[nodeIndex];
			attributes.push_back(asAttribute(node, attributeName));
		}

		xmlXPathFreeObject(resultToBeFreed);
		resultToBeFreed = nullptr;

		return attributes;
	}
	catch (runtime_error &e)
	{
		if (resultToBeFreed != nullptr)
			xmlXPathFreeObject(resultToBeFreed);

		if (emptyOnError)
			return vector<string>();
		else
		{
			SPDLOG_ERROR(
				"asAttributesList failed"
				", xPathExpression: {}"
				", e.what(): {}",
				xPathExpression, e.what()
			);

			throw e;
		}
	}
	catch (exception &e)
	{
		SPDLOG_ERROR(
			"asAttributesList failed"
			", xPathExpression: {}",
			xPathExpression
		);

		if (resultToBeFreed != nullptr)
			xmlXPathFreeObject(resultToBeFreed);

		throw e;
	}
}

vector<string> XMLWrapper::asTextList(string xPathExpression, xmlNodePtr startingNode, bool emptyOnError)
{
	xmlXPathObjectPtr resultToBeFreed = nullptr;
	try
	{
		resultToBeFreed = xPath(xPathExpression, startingNode, emptyOnError);

		vector<string> textList;
		for (int nodeIndex = 0; nodeIndex < resultToBeFreed->nodesetval->nodeNr; nodeIndex++)
		{
			xmlNodePtr node = resultToBeFreed->nodesetval->nodeTab[nodeIndex];
			if (node->type == XML_ELEMENT_NODE)
				textList.push_back((char *)(node->children->content));
			else // XML_TEXT_NODE
				textList.push_back((char *)(node->content));
		}

		xmlXPathFreeObject(resultToBeFreed);
		resultToBeFreed = nullptr;

		return textList;
	}
	catch (runtime_error &e)
	{
		if (resultToBeFreed != nullptr)
			xmlXPathFreeObject(resultToBeFreed);

		if (emptyOnError)
			return vector<string>();
		else
		{
			SPDLOG_ERROR(
				"asAttributes failed"
				", xPathExpression: {}"
				", e.what(): {}",
				xPathExpression, e.what()
			);

			throw e;
		}
	}
	catch (exception &e)
	{
		SPDLOG_ERROR(
			"asAttributes failed"
			", xPathExpression: {}",
			xPathExpression
		);

		if (resultToBeFreed != nullptr)
			xmlXPathFreeObject(resultToBeFreed);

		throw e;
	}
}

string XMLWrapper::asText(string xPathExpression, xmlNodePtr startingNode, bool emptyOnError)
{
	xmlXPathObjectPtr resultToBeFreed = nullptr;
	try
	{
		resultToBeFreed = xPath(xPathExpression, startingNode, emptyOnError);
		string text;
		if (resultToBeFreed->type == XPATH_STRING)
			text = (char *)resultToBeFreed->stringval;
		else if (resultToBeFreed->type == XPATH_NODESET && resultToBeFreed->nodesetval->nodeNr > 0)
		{
			if (resultToBeFreed->nodesetval->nodeTab[0]->type == XML_TEXT_NODE)
				text = (char *)resultToBeFreed->nodesetval->nodeTab[0]->content;
			else if (resultToBeFreed->nodesetval->nodeTab[0]->type == XML_ATTRIBUTE_NODE)
				text = (char *)resultToBeFreed->nodesetval->nodeTab[0]->children->content;
		}
		else
		{
			string errorMessage = std::format(
				"xPathExpression was found but how to retrieve the text?"
				", xPathExpression: {}"
				", type: {}",
				xPathExpression, (int)(resultToBeFreed->type)
			);
			SPDLOG_ERROR(errorMessage);

			// resultToBeFreed will be freed into the catch

			throw runtime_error(errorMessage);
		}

		xmlXPathFreeObject(resultToBeFreed);
		resultToBeFreed = nullptr;

		return text;
	}
	catch (runtime_error &e)
	{
		if (resultToBeFreed != nullptr)
			xmlXPathFreeObject(resultToBeFreed);

		if (emptyOnError)
			return "";
		else
		{
			SPDLOG_ERROR(
				"asText failed"
				", xPathExpression: {}"
				", e.what(): {}",
				xPathExpression, e.what()
			);

			throw e;
		}
	}
	catch (exception &e)
	{
		SPDLOG_ERROR(
			"asText failed"
			", xPathExpression: {}",
			xPathExpression
		);

		if (resultToBeFreed != nullptr)
			xmlXPathFreeObject(resultToBeFreed);

		throw e;
	}
}

bool XMLWrapper::tagExist(string xPathExpression, xmlNodePtr startingNode, bool emptyOnError)
{
	xmlXPathObjectPtr resultToBeFreed = nullptr;
	try
	{
		bool tagExist = false;

		resultToBeFreed = xPath(xPathExpression, startingNode, emptyOnError);
		if (resultToBeFreed->type == XPATH_NODESET && resultToBeFreed->nodesetval->nodeNr > 0)
			tagExist = true;

		xmlXPathFreeObject(resultToBeFreed);
		resultToBeFreed = nullptr;

		return tagExist;
	}
	catch (runtime_error &e)
	{
		if (resultToBeFreed != nullptr)
			xmlXPathFreeObject(resultToBeFreed);

		if (emptyOnError)
			return false;
		else
		{
			SPDLOG_ERROR(
				"tagExist failed"
				", xPathExpression: {}"
				", e.what(): {}",
				xPathExpression, e.what()
			);

			throw e;
		}
	}
	catch (exception &e)
	{
		SPDLOG_ERROR(
			"tagExist failed"
			", xPathExpression: {}",
			xPathExpression
		);

		if (resultToBeFreed != nullptr)
			xmlXPathFreeObject(resultToBeFreed);

		throw e;
	}
}

void XMLWrapper::logAttributes(xmlNodePtr node)
{
	try
	{
		xmlAttr *attribute = node->properties;
		while (attribute)
		{
			xmlChar *value = xmlNodeListGetString(node->doc, attribute->children, 1);
			SPDLOG_INFO("{}: {}", (char *)attribute->name, (char *)value);
			xmlFree(value);
			attribute = attribute->next;
		}
	}
	catch (exception &e)
	{
		SPDLOG_ERROR(
			"logAttributes failed"
			", node->name: {}"
			", e.what(): {}",
			(node == nullptr ? "" : (char *)node->name), e.what()
		);

		throw;
	}
}

string XMLWrapper::toString()
{
	if (_doc == nullptr)
	{
		string errorMessage = std::format("Document not initialized");
		SPDLOG_ERROR(errorMessage);

		throw runtime_error(errorMessage);
	}

	string out;

	xmlChar *s;
	int size;
	// xmlDocDumpFormatMemoryEnc(pDoc, &psOutput, &iSize, "UTF-8", 1);
	xmlDocDumpMemory(_doc, &s, &size);
	if (s == NULL)
	{
		SPDLOG_ERROR("xmlDocDumpMemory failed");
		throw bad_alloc();
	}

	try
	{
		out = (char *)s;
	}
	catch (exception &e)
	{
		SPDLOG_ERROR(
			"xmlDocDumpMemory failed"
			", e.what(): {}",
			e.what()
		);
		xmlFree(s);

		throw;
	}

	xmlFree(s);

	return out;
}

string XMLWrapper::nodeToString(xmlNodePtr node)
{
	xmlBufferPtr buffer = xmlBufferCreate();
	if (!buffer)
		return "";

	// Dump del nodo nel buffer
	xmlNodeDump(buffer, node->doc, node, 0, 1); // indentazione = 1

	string out;
	if (buffer->content)
	{
		out = reinterpret_cast<const char *>(buffer->content);
	}

	xmlBufferFree(buffer);
	return out;
}
