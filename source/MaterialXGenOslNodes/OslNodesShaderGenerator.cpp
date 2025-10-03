//
// Copyright Contributors to the MaterialX Project
// SPDX-License-Identifier: Apache-2.0
//

#include <MaterialXGenOslNodes/OslNodesShaderGenerator.h>
#include <MaterialXGenOslNodes/OslNodesSyntax.h>

#include <MaterialXGenShader/GenContext.h>
#include <MaterialXGenShader/Shader.h>
#include <MaterialXGenShader/TypeDesc.h>
#include <MaterialXGenShader/ShaderStage.h>
#include <MaterialXGenShader/Nodes/SourceCodeNode.h>


MATERIALX_NAMESPACE_BEGIN

const string OslNodesShaderGenerator::TARGET = "genoslnodes";

//
// OslNodesShaderGenerator methods
//

OslNodesShaderGenerator::OslNodesShaderGenerator(TypeSystemPtr typeSystem) :
    ShaderGenerator(typeSystem, OslNodesSyntax::create(typeSystem))
{
}

static string paramString(const string& paramType, const string& paramName, const string& paramValue)
{
    return "param " + paramType + " " + paramName + " " + paramValue + " ;";
}

static string connectString(const string& fromNode, const string& fromName, const string& toNode, const string& toName)
{
    return "connect " + fromNode + "." + fromName + " " + toNode + "." + toName + " ;";
}

ShaderPtr OslNodesShaderGenerator::generate(const string& name, ElementPtr element, GenContext& context) const
{
    ShaderPtr shader = createShader(name, element, context);
    ShaderGraph& graph = shader->getGraph();
    ShaderStage& stage = shader->getStage(Stage::PIXEL);

    ConstDocumentPtr document = element->getDocument();

    string lastNodeName;
    ShaderOutput* lastOutput;
    std::vector<string> connections;

    std::set<std::string> osoPaths;

    // Walk the node graph, emitting shaders and param declarations.
    for (auto&& node : graph.getNodes()) {
        const string& name = node->getName();

        for (auto&& input : node->getInputs()) {
            if (input->isDefault())
                continue;

            string inputName = input->getName();
            _syntax->makeValidName(inputName);

            const ShaderOutput* connection = input->getConnection();
            if (!connection || connection->getNode() == &graph) {
                if (input->getName() == "backsurfaceshader"
                    || input->getName() == "displacementshader")
                    continue; // FIXME: these aren't getting pruned by isDefault

                string value = _syntax->getValue(input);
                if (value == "null_closure()")
                    continue;

                emitLine(paramString(_syntax->getTypeName(input->getType()), inputName, value), stage, false);
            } else {
                string connName = connection->getName();
                _syntax->makeValidName(connName);

                string connect = connectString(connection->getNode()->getName(), connName,  name, inputName);
                // Save connect emits for the end, because they can't come
                // before both connected shaders have been declared.
                connections.push_back(connect);
            }
        }

        // Keep track of the root output, so we can connect it to our setCi node
        lastOutput = node->getOutput(0);

        NodeDefPtr nodeDef = document->getNodeDef(node->getNodeDefName());
        ImplementationPtr impl = nodeDef->getImplementation("genoslnodes")->asA<Implementation>();

        if (!impl)
        {
            printf("Skipping test due to missing implementation\n");
            return nullptr;
        }

        string osoName = impl->getFunction();

        string osoPath = impl->getFile();
        osoPaths.insert(osoPath);

        emitLine("shader " + osoName + " " + name + " ;", stage, false);
        lastNodeName = name;
    }

    for (auto&& connect : connections) {
        emitLine(connect, stage, false);
    }

    // During unit tests, wrap a special node that will add the output to Ci.
    if (context.getOptions().oslNodesConnectCiWrapper) {
        emitLine("shader setCi root ;", stage, false);
        string connect = connectString(
            lastNodeName,
            lastOutput->getName(),
            "root",
            lastOutput->getType().getName() + "_input"
        );
        emitLine(connect, stage, false);
    }

    // From our set of required oso paths, build the path string that oslc will need.
    string osoPathStr;
    string separator = "";
    for (const auto& osoPath : osoPaths)
    {
        auto fullOsoPath = context.resolveSourceFile(osoPath, "");
        auto fullOsoPathStr = fullOsoPath.asString();

        osoPathStr += separator + fullOsoPathStr;
        separator = ",";
    }

    shader->setAttribute("osoPath", Value::createValue<string>(osoPathStr));

    return shader;
}


ShaderPtr OslNodesShaderGenerator::createShader(const string& name, ElementPtr element, GenContext& context) const
{
    // Create the root shader graph
    ShaderGraphPtr graph = ShaderGraph::create(nullptr, name, element, context);
    ShaderPtr shader = std::make_shared<Shader>(name, graph);

    // Create our stage.
    ShaderStagePtr stage = createStage(Stage::PIXEL, *shader);
    stage->createUniformBlock(OSLNodes::UNIFORMS);
    stage->createInputBlock(OSLNodes::INPUTS);
    stage->createOutputBlock(OSLNodes::OUTPUTS);

    // Create shader variables for all nodes that need this.
    createVariables(graph, context, *shader);

    // Create uniforms for the published graph interface.
    VariableBlock& uniforms = stage->getUniformBlock(OSLNodes::UNIFORMS);
    for (ShaderGraphInputSocket* inputSocket : graph->getInputSockets())
    {
        // Only for inputs that are connected/used internally,
        // and are editable by users.
        if (inputSocket->getConnections().size() && graph->isEditable(*inputSocket))
        {
            uniforms.add(inputSocket->getSelf());
        }
    }

    // Create outputs from the graph interface.
    VariableBlock& outputs = stage->getOutputBlock(OSLNodes::OUTPUTS);
    for (ShaderGraphOutputSocket* outputSocket : graph->getOutputSockets())
    {
        outputs.add(outputSocket->getSelf());
    }

    return shader;
}


namespace OSLNodes
{

// Identifiers for OSL variable blocks
const string UNIFORMS = "u";
const string INPUTS = "i";
const string OUTPUTS = "o";

} // namespace OSL

MATERIALX_NAMESPACE_END
