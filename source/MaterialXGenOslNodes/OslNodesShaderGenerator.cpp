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

const string OslNodesShaderGenerator::TARGET = "genosl";

//
// OslNodesShaderGenerator methods
//

OslNodesShaderGenerator::OslNodesShaderGenerator(TypeSystemPtr typeSystem) :
    ShaderGenerator(typeSystem, OslNodesSyntax::create(typeSystem))
{
}

ShaderPtr OslNodesShaderGenerator::generate(const string& name, ElementPtr element, GenContext& context) const
{
    ShaderPtr shader = createShader(name, element, context);

    // Request fixed floating-point notation for consistency across targets.
    ScopedFloatFormatting fmt(Value::FloatFormatFixed);

    ShaderGraph& graph = shader->getGraph();
    ShaderStage& stage = shader->getStage(Stage::PIXEL);

    std::vector<string> connections;
    for (auto&& node : graph.getNodes()) {
        const string& name = node->getName();

        for (auto&& input : node->getInputs()) {
            const string& inputName = input->getName();
            TypeDesc inputType = input->getType();

            if (!input->getValue())
                continue;

            if (input->isDefault())
                continue;

            const ShaderOutput* connection = input->getConnection();
            if (connection->getNode() == &graph) {
                if (input->getName() == "backsurfaceshader"
                    || input->getName() == "displacementshader")
                    continue; // FIXME: these aren't getting pruned by isDefault

                emitLine("param " + _syntax->getTypeName(inputType) + " " + input->getName() + " " + _syntax->getValue(input) + " ;", stage, false);
            } else {
                string connect = "connect " + connection->getNode()->getName() + "." + connection->getName() + " " + name + "." + inputName + " ;";
                connections.push_back(connect);
            }
        }

        string nodeDefName = node->getNodeDefName();
        // Remove the "ND_" prefix from a valid `NodeDef` name.
        if (nodeDefName.size() > 3 && nodeDefName.substr(0, 3) == "ND_")
        {
            nodeDefName = nodeDefName.substr(3);
        }

        emitLine("shader " + nodeDefName + " " + name + " ;", stage, false);
    }

    for (auto&& connect : connections) {
        emitLine(connect, stage, false);
    }

    return shader;
}


ShaderPtr OslNodesShaderGenerator::createShader(const string& name, ElementPtr element, GenContext& context) const
{
    // Create the root shader graph
    ShaderGraphPtr graph = ShaderGraph::create(nullptr, name, element, context);
    ShaderPtr shader = std::make_shared<Shader>(name, graph);

    // Create our stage.
    ShaderStagePtr stage = createStage(Stage::PIXEL, *shader);
    stage->createUniformBlock(OSL::UNIFORMS);
    stage->createInputBlock(OSL::INPUTS);
    stage->createOutputBlock(OSL::OUTPUTS);

    // Create shader variables for all nodes that need this.
    createVariables(graph, context, *shader);

    // Create uniforms for the published graph interface.
    VariableBlock& uniforms = stage->getUniformBlock(OSL::UNIFORMS);
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
    VariableBlock& outputs = stage->getOutputBlock(OSL::OUTPUTS);
    for (ShaderGraphOutputSocket* outputSocket : graph->getOutputSockets())
    {
        outputs.add(outputSocket->getSelf());
    }

    return shader;
}


namespace OSL
{

// Identifiers for OSL variable blocks
const string UNIFORMS = "u";
const string INPUTS = "i";
const string OUTPUTS = "o";

} // namespace OSL

MATERIALX_NAMESPACE_END
