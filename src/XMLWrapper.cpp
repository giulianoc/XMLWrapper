
#include "XMLWrapper.h"
#include "CurlWrapper.h"
#include <exception>
#include <libxml/xpathInternals.h>
#include <regex>

using namespace std;

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
	const string& url, long timeoutInSeconds, const string& basicAuthenticationUser, const string& basicAuthenticationPassword,
	const vector<string>& otherHeaders,
	int maxRetryNumber, int secondsToWaitBeforeToRetry, const vector<pair<string, string>>& nameServices
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
			LOG_ERROR(errorMessage);

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
			LOG_ERROR(errorMessage);

			throw runtime_error(errorMessage);
		}

		for (const pair<string, string>& nameService : nameServices)
			xmlXPathRegisterNs(_xpathCtx, BAD_CAST nameService.first.c_str(), BAD_CAST nameService.second.c_str());
	}
	catch (const exception &e)
	{
		LOG_ERROR(
			"loadXML failed"
			", url: {}"
			", exception: {}",
			url, e.what()
		);

		finish();

		throw;
	}
}

string XMLWrapper::asString(const bool pretty) const
{
	xmlChar* mem = nullptr;
	try
	{
		if (!_doc)
		{
			const string errorMessage = "Document not initialized";
			LOG_ERROR(errorMessage);

			throw runtime_error(errorMessage);
		}

		// Assicura versione/encoding (così libxml2 emette la declaration)
		if (!_doc->version)
			_doc->version = xmlStrdup(BAD_CAST "1.0");

		// Imposta l'encoding: xmlCharEncodingHandler viene scelto dal nome ("UTF-8")
		// Nota: doc->encoding è xmlChar*, va allocata con funzioni libxml2.
		if (!_doc->encoding)
			_doc->encoding = xmlStrdup(BAD_CAST "UTF-8");

		int size = 0;
		if (pretty)
			xmlDocDumpFormatMemoryEnc(_doc, &mem, &size, "UTF-8", 1);
		else
			xmlDocDumpMemoryEnc(_doc, &mem, &size, "UTF-8");
		if (!mem || size <= 0)
		{
			const string errorMessage = std::format(
				"xmlDocDumpMemoryEnc failed"
				", size: {}", size
				);
			LOG_ERROR(errorMessage);

			if (mem)
				xmlFree(mem);

			throw runtime_error(errorMessage);
		}

		std::string sXML(reinterpret_cast<char*>(mem), static_cast<size_t>(size));
		xmlFree(mem);

		return sXML;
	}
	catch (const exception &e)
	{
		LOG_ERROR(
			"asString failed"
			", exception: {}",
			e.what()
		);

		if (mem)
			xmlFree(mem);

		throw;
	}
}

void XMLWrapper::saveXML(string pathName, bool pretty) const
{
	try
	{
		string sXML = asString(pretty);

		std::ofstream out(pathName, std::ios::binary); // binary evita conversioni newline
		if (!out)
		{
			const string errorMessage = std::format("Creation of file failed"
				", pathName: {}", pathName
				);
			LOG_ERROR(errorMessage);

			throw runtime_error(errorMessage);
		}

		out.write(sXML.data(), static_cast<std::streamsize>(sXML.size()));
	}
	catch (const exception &e)
	{
		LOG_ERROR(
			"saveXML failed"
			", pathName: {}"
			", exception: {}",
			pathName, e.what()
		);

		throw;
	}
}

xmlNodePtr XMLWrapper::asRootNode() const
{
	try
	{
		if (_doc == nullptr)
		{
			string errorMessage = std::format("Document not initialized");
			LOG_ERROR(errorMessage);

			throw runtime_error(errorMessage);
		}

		return xmlDocGetRootElement(_doc);
	}
	catch (const exception &e)
	{
		LOG_ERROR(
			"asRootNode failed"
			", exception: {}",
			e.what()
		);

		throw;
	}
}

xmlXPathObjectPtr XMLWrapper::xPath(const string& xPathExpression, xmlNodePtr startingNode, bool noErrorLog) const
{
	xmlXPathObjectPtr resultToBeFreed = nullptr;
	try
	{
		if (_doc == nullptr)
		{
			string errorMessage = std::format("Document not initialized");
			LOG_ERROR(errorMessage);

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
					", xPathExpression: {}", // ", nodeDump: {}",
					xPathExpression			 //, nodeToString(startingNode)
				);
				if (!noErrorLog)
					LOG_ERROR(errorMessage);

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
			LOG_INFO("xmlXPathEvalExpression"
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
	catch (const exception &e)
	{
		if (!noErrorLog)
			LOG_ERROR(
				"xPath failed"
				", xPathExpression: {}"
				", exception: {}",
				xPathExpression, e.what()
			);

		if (resultToBeFreed != nullptr)
			xmlXPathFreeObject(resultToBeFreed);

		throw;
	}
}

string XMLWrapper::asAttribute(xmlNodePtr node, const string& attributeName, bool emptyOnError)
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
		string sAttributeValue = reinterpret_cast<char *>(attributeValue);
		xmlFree(attributeValue);
		attributeValue = nullptr;

		return sAttributeValue;
	}
	catch (const exception &e)
	{
		if (attributeValue != nullptr)
			xmlFree(attributeValue);

		if (emptyOnError)
			return "";

		LOG_ERROR(
			"asAttribute failed"
			", node->name: {}"
			", attributeName: {}",
			node == nullptr ? "" : (char *)node->name, attributeName
		);

		throw;
	}
}

string XMLWrapper::asAttribute(string xPathExpression, const string& attributeName, xmlNodePtr startingNode, bool emptyOnError) const
{
	xmlXPathObjectPtr resultToBeFreed = nullptr;
	try
	{
		resultToBeFreed = xPath(xPathExpression, startingNode, emptyOnError);
	}
	catch (const exception &e)
	{
		if (!emptyOnError)
			LOG_ERROR(
				"asAttribute failed"
				", xPathExpression: {}"
				", exception: {}",
				xPathExpression, e.what()
			);

		if (emptyOnError)
			return "";
		throw;
	}

	try
	{
		string attributeValue = asAttribute(resultToBeFreed->nodesetval->nodeTab[0], attributeName, emptyOnError);

		xmlXPathFreeObject(resultToBeFreed);
		resultToBeFreed = nullptr;

		return attributeValue;
	}
	catch (const exception &e)
	{
		if (!emptyOnError)
			LOG_ERROR(
				"asAttribute failed"
				", xPathExpression: {}"
				", exception: {}",
				xPathExpression, e.what()
			);

		xmlXPathFreeObject(resultToBeFreed);

		throw;
	}
}

bool XMLWrapper::setAttribute(const std::string& xPathExpression,
	const size_t nodeIndex, // starting from 0
	const std::string& attributeName,
	const std::string& attributeValue,
	xmlNodePtr startingNode) const
{
    xmlXPathObjectPtr resultToBeFreed = nullptr;

    try
    {
        resultToBeFreed = xPath(xPathExpression, startingNode, false);
    }
    catch (const std::exception& e)
    {
        LOG_ERROR("setAttribute failed"
            ", xPathExpression: {}"
            ", exception: {}",
            xPathExpression, e.what());

        throw;
    }

    try
    {
        if (!resultToBeFreed
        	|| !resultToBeFreed->nodesetval
        	|| resultToBeFreed->nodesetval->nodeNr <= nodeIndex)
        {
        	string errorMessage = std::format("setAttribute failed, node not found"
				", xPathExpression: {}",
				xPathExpression);
        	LOG_ERROR(errorMessage);

        	if (resultToBeFreed)
                xmlXPathFreeObject(resultToBeFreed);

            throw std::runtime_error(errorMessage);
        }

        xmlNodePtr node = resultToBeFreed->nodesetval->nodeTab[nodeIndex];
        if (!node)
        {
			const string errorMessage = std::format("setAttribute failed, node not found"
				", xPathExpression: {}",
				xPathExpression);
        	LOG_ERROR(errorMessage);

        	xmlXPathFreeObject(resultToBeFreed);
            throw std::runtime_error(errorMessage);
        }

        // Set (create or replace) the attribute.
        // xmlSetProp returns xmlAttrPtr (nullptr on error).
        xmlAttrPtr a = xmlSetProp(node, BAD_CAST attributeName.c_str(), BAD_CAST attributeValue.c_str());

        xmlXPathFreeObject(resultToBeFreed);
        resultToBeFreed = nullptr;

        if (!a)
        {
        	string errorMessage = std::format("setAttribute failed"
				", xPathExpression: {}"
				", attributeName: {}",
				xPathExpression, attributeName);
        	LOG_ERROR(errorMessage);

            throw std::runtime_error(errorMessage);
        }

        return true;
    }
    catch (const std::exception& e)
    {
        LOG_ERROR("setAttribute failed"
        	", xPathExpression: {}"
        	", exception: {}",
        	xPathExpression, e.what());

        if (resultToBeFreed)
            xmlXPathFreeObject(resultToBeFreed);

        throw;
    }
}

vector<string> XMLWrapper::asAttributesList(const string& xPathExpression, const string& attributeName, xmlNodePtr startingNode, bool emptyOnError) const
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
	catch (const exception &e)
	{
		if (resultToBeFreed != nullptr)
			xmlXPathFreeObject(resultToBeFreed);

		if (emptyOnError)
			return {};

		LOG_ERROR(
			"asAttributesList failed"
			", xPathExpression: {}"
			", exception: {}",
			xPathExpression, e.what()
		);

		throw;
	}
}

vector<string> XMLWrapper::asTextList(const string& xPathExpression, xmlNodePtr startingNode, bool emptyOnError) const
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
	catch (const exception &e)
	{
		if (resultToBeFreed != nullptr)
			xmlXPathFreeObject(resultToBeFreed);

		if (emptyOnError)
			return {};

		LOG_ERROR(
			"asAttributes failed"
			", xPathExpression: {}"
			", exception: {}",
			xPathExpression, e.what()
		);

		throw;
	}
}

string XMLWrapper::asText(const string& xPathExpression, xmlNodePtr startingNode, bool emptyOnError) const
{
	xmlXPathObjectPtr resultToBeFreed = nullptr;
	try
	{
		resultToBeFreed = xPath(xPathExpression, startingNode, emptyOnError);
		string text;
		if (resultToBeFreed->type == XPATH_STRING)
			text = reinterpret_cast<char *>(resultToBeFreed->stringval);
		else if (resultToBeFreed->type == XPATH_NODESET && resultToBeFreed->nodesetval->nodeNr > 0)
		{
			if (resultToBeFreed->nodesetval->nodeTab[0]->type == XML_TEXT_NODE)
				text = reinterpret_cast<char *>(resultToBeFreed->nodesetval->nodeTab[0]->content);
			else if (resultToBeFreed->nodesetval->nodeTab[0]->type == XML_ATTRIBUTE_NODE)
				text = reinterpret_cast<char *>(resultToBeFreed->nodesetval->nodeTab[0]->children->content);
		}
		else
		{
			string errorMessage = std::format(
				"xPathExpression was found but how to retrieve the text?"
				", xPathExpression: {}"
				", type: {}",
				xPathExpression, static_cast<int>(resultToBeFreed->type)
			);
			LOG_ERROR(errorMessage);

			// resultToBeFreed will be freed into the catch

			throw runtime_error(errorMessage);
		}

		xmlXPathFreeObject(resultToBeFreed);
		resultToBeFreed = nullptr;

		return text;
	}
	catch (const exception &e)
	{
		if (resultToBeFreed != nullptr)
			xmlXPathFreeObject(resultToBeFreed);

		if (emptyOnError)
			return "";

		LOG_ERROR(
			"asText failed"
			", xPathExpression: {}"
			", exception: {}",
			xPathExpression, e.what()
		);

		throw;
	}
}

bool XMLWrapper::tagExist(const string& xPathExpression, xmlNodePtr startingNode, bool emptyOnError) const
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
	catch (const exception &e)
	{
		if (resultToBeFreed != nullptr)
			xmlXPathFreeObject(resultToBeFreed);

		if (emptyOnError)
			return false;

		LOG_ERROR(
			"tagExist failed"
			", xPathExpression: {}"
			", exception: {}",
			xPathExpression, e.what()
		);

		throw;
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
			LOG_INFO("{}: {}", (char *)attribute->name, (char *)value);
			xmlFree(value);
			attribute = attribute->next;
		}
	}
	catch (const exception &e)
	{
		LOG_ERROR(
			"logAttributes failed"
			", node->name: {}"
			", exception: {}",
			(node == nullptr ? "" : (char *)node->name), e.what()
		);

		throw;
	}
}

/*
string XMLWrapper::toString() const
{
	if (_doc == nullptr)
	{
		string errorMessage = std::format("Document not initialized");
		LOG_ERROR(errorMessage);

		throw runtime_error(errorMessage);
	}

	string out;

	xmlChar *s;
	int size;
	// xmlDocDumpFormatMemoryEnc(pDoc, &psOutput, &iSize, "UTF-8", 1);
	xmlDocDumpMemory(_doc, &s, &size);
	if (s == NULL)
	{
		LOG_ERROR("xmlDocDumpMemory failed");
		throw bad_alloc();
	}

	try
	{
		out = reinterpret_cast<char *>(s);
	}
	catch (exception &e)
	{
		LOG_ERROR(
			"xmlDocDumpMemory failed"
			", exception: {}",
			e.what()
		);
		xmlFree(s);

		throw;
	}

	xmlFree(s);

	return out;
}
*/

string XMLWrapper::nodeToString(xmlNodePtr node)
{
	xmlBufferPtr buffer = xmlBufferCreate();
	if (!buffer)
		return "";

	// Dump del nodo nel buffer
	xmlNodeDump(buffer, node->doc, node, 0, 1); // indentazione = 1

	string out;
	if (buffer->content)
		out = reinterpret_cast<const char *>(buffer->content);

	xmlBufferFree(buffer);
	return out;
}
