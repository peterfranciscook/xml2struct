#include <algorithm>
#include <string.h>
// #include <cstring>
#include <vector>
#include "pugixml.hpp"
#include "mex.h"

using namespace pugi;

const char* node_types[] = { "null", "document", "element", "pcdata", "cdata", "comment", "pi", "declaration" };

bool hasChildNodes(pugi::xml_node& node)
{
    return node.first_child() == NULL ? false : true;
}

std::vector<pugi::xml_node> getChildNodes(pugi::xml_node& node)
{
    std::vector<pugi::xml_node> childNodes;
    int pos;
    node = node.first_child();
    std::string tempName;

    while (node != NULL)
    {
        tempName = node.name();
        while ((pos = tempName.find(":")) != std::string::npos)
        {
            tempName.replace(pos, 1, "_colon_");
        }
        while ((pos = tempName.find(".")) != std::string::npos)
        {
            tempName.replace(pos, 1, "_dot_");
        }
        while ((pos = tempName.find("-")) != std::string::npos)
        {
            tempName.replace(pos, 1, "_dash_");
        }
        node.set_name(tempName.c_str());
//         mexPrintf("%s\n", node.name());
        
        childNodes.push_back(node);
        node = node.next_sibling();
    }

    return childNodes;
}

std::vector<std::string> getDistinctNodeNames(std::vector<std::string> nodes)
{
    std::vector<std::string> distinctNames;

    for (int i = 0; i < nodes.size(); i++)
    {
        if (!(std::find(distinctNames.begin(), distinctNames.end(), nodes.at(i)) != distinctNames.end()))
        {
            distinctNames.push_back(nodes.at(i));
        }
    }

    return distinctNames;
}

mxArray *parseAttributes(pugi::xml_node& node)
{
    mxArray *attributes = NULL;
    pugi::xml_attribute attr = node.first_attribute();
    int pos;
    
    if (attr != NULL)
    {
        std::string tempName = attr.name();
        
        while ((pos = tempName.find(":")) != std::string::npos)
        {
            tempName.replace(pos, 1, "_colon_");
        }
        while ((pos = tempName.find(".")) != std::string::npos)
        {
            tempName.replace(pos, 1, "_dot_");
        }
        while ((pos = tempName.find("-")) != std::string::npos)
        {
            tempName.replace(pos, 1, "_dash_");
        }
        
        const char *attributeName[1] = {tempName.c_str()};
        mxArray *temp = mxCreateStructMatrix(1, 1, 1, attributeName);
        mxArray *attrValue = mxCreateString(attr.value());
        mxSetField(temp, 0, attributeName[0], attrValue);

        for (attr = attr.next_attribute(); attr; attr = attr.next_attribute())
        {
            std::string tempName = attr.name();

            while ((pos = tempName.find(":")) != std::string::npos)
            {
                tempName.replace(pos, 1, "_colon_");
            }
            while ((pos = tempName.find(".")) != std::string::npos)
            {
                tempName.replace(pos, 1, "_dot_");
            }
            while ((pos = tempName.find("-")) != std::string::npos)
            {
                tempName.replace(pos, 1, "_dash_");
            }

            const char *attrFieldName = tempName.c_str();
            mxArray *attrValue = mxCreateString(attr.value());
            mxAddField(temp, attrFieldName);
            mxSetField(temp, 0, attrFieldName, attrValue);
        }

        attributes = temp;
    }

    return attributes;
}

mxArray* parseChildNodes(pugi::xml_node& node)
{
    mxArray *children = NULL;
    
    if (hasChildNodes(node))
    {
        mxArray *attributes = parseAttributes(node);

        std::vector<std::string> distinctNames;
        std::vector<std::string> allChildNodeNames;
        std::vector<pugi::xml_node> childNodes;
        int numChildNodes;

        childNodes = getChildNodes(node);
        numChildNodes = childNodes.size();

        for (int i = 0; i < numChildNodes; i++)
        {
            allChildNodeNames.push_back(childNodes.at(i).name());
        }
        
        distinctNames = getDistinctNodeNames(allChildNodeNames);
        const char *distinctChildNodeNames[distinctNames.size()] = {};

        for (int i = 0; i < distinctNames.size(); i++)
        {
            distinctChildNodeNames[i] = distinctNames.at(i).c_str();
        }
        
//         /* Patch for bypassing the variable-length arrays problems of modern C++ compilers */
//         std::vector<const char*> distinctChildNodeNames;
//         std::transform(distinctNames.begin(), distinctNames.end(), std::back_inserter(distinctChildNodeNames), [](const std::string& str) {
//             // initialize empty char array
//             char *output = new char[str.size()+1];
//             std::strcpy(output, str.c_str());
//             return output;
//         });
        
        std::vector<std::string> processedNames;

        children = mxCreateStructMatrix(1, 1, distinctNames.size(), distinctChildNodeNames);

        for (int idx = 0; idx < childNodes.size(); idx++)
        {
            pugi::xml_node theChild = childNodes.at(idx);
            std::string type = node_types[theChild.type()];
            std::string temp = theChild.name();
            std::string val = theChild.value();

            const char *namey[1] = {};
            namey[0] = temp.c_str();
            mxArray *glhf = mxGetField(children, 0, namey[0]);
            int indexOfMatchingItem = mxGetFieldNumber(children,  namey[0]);
            
            if (!(strcmp(type.c_str(), "pcdata") == 0) && !(strcmp(type.c_str(), "comment") == 0) && !(strcmp(type.c_str(), "cdata") == 0))
            {
                //XML allows the same elements to be defined multiple times, put each in a different cell
                if (std::find(processedNames.begin(), processedNames.end(), temp) != processedNames.end())
                {
                    if ( glhf != NULL )
                    {
                        if (!mxIsCell(glhf))
                        {
                            mxArray *temp = glhf;
                            glhf = mxCreateCellMatrix(1, 2);
                            mxSetCell(glhf, 0, temp);
                            mxSetCell(glhf, 1, parseChildNodes(theChild));
                            mxSetCell(children, indexOfMatchingItem, glhf);
                        }
                        else
                        {
                            int numberItemsInCell = mxGetN(glhf);
                            mxArray *temp = glhf;
                            glhf = mxCreateCellMatrix(1, numberItemsInCell + 1);

                            for (int i = 0; i < numberItemsInCell; i++)
                            {
                                mxSetCell(glhf, i, mxGetCell(temp, i));
                            }

                            mxSetCell(glhf, numberItemsInCell, parseChildNodes(theChild));
                            mxSetCell(children, indexOfMatchingItem, glhf);
                        }
                    }
                }
                //add previously unknown (new) element to the structure
                else
                {
                    mxSetCell(children, indexOfMatchingItem, parseChildNodes(theChild));
                }

                processedNames.push_back(temp);
            }
            else
            {
                const char *typeFieldNames[1] = {"Text"};
                std::string value  = theChild.value();
                mxArray *matValue  = mxCreateString(value.c_str());

                if (strcmp(type.c_str(), "cdata") == 0)
                {
                    typeFieldNames[0] = "CDATA";
                }
                else if (strcmp(type.c_str(), "comment") == 0)
                {
                    typeFieldNames[0] = "Comment";
                }

                children = mxCreateStructMatrix(1, 1, 1, typeFieldNames);
                mxSetFieldByNumber(children, 0, 0, matValue);

                processedNames.push_back(temp);
            }
        }

        if (attributes != NULL)
        {
            const char *attrFieldName = "Attributes";
            mxAddField(children, attrFieldName);
            mxSetField(children, 0, attrFieldName, attributes);
        }

    }

    return children;
}

void  mexFunction(int nlhs, mxArray *plhs[], int nrhs, const mxArray *prhs[])
{
    char *input_buf;
    size_t buflen;

    if(nrhs!=1)
    mexErrMsgIdAndTxt( "MATLAB:revord:invalidNumInputs", "One input required.");

    if ( mxIsChar(prhs[0]) != 1)
    mexErrMsgIdAndTxt( "MATLAB:revord:inputNotString", "Input must be a string.");

    buflen = (mxGetM(prhs[0]) * mxGetN(prhs[0])) + 1;
    input_buf = mxArrayToString(prhs[0]);

    pugi::xml_document doc;
    pugi::xml_parse_result result = doc.load_file(input_buf);

    if (!result)
    {
        mexPrintf("Error description: %s\n", result.description());

        mexErrMsgIdAndTxt( "MATLAB:revord:fileIssue", "There was an issue with the given file.");
    }
    else
    {
        pugi::xml_node topNode = doc;
        mxArray *vin1 = parseChildNodes(topNode);
        plhs[0] = vin1;
    }
}