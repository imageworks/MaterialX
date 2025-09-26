//
// Copyright Contributors to the MaterialX Project
// SPDX-License-Identifier: Apache-2.0
//

#include <fstream>
#include <iostream>

#include <MaterialXCore/Document.h>
#include <MaterialXCore/Util.h>

#include <MaterialXFormat/File.h>
#include <MaterialXFormat/Util.h>

#include <MaterialXGenShader/ShaderStage.h>

#include <MaterialXGenOsl/OslShaderGenerator.h>
#include <MaterialXGenOsl/OslSyntax.h>

#include <MaterialXRenderOsl/OslRenderer.h>

namespace mx = MaterialX;

const std::string options =
    "    Options: \n"
    "        --outputPath [DIRPATH]          TODO\n"
    "        --oslCompilerPath [FILEPATH]    TODO\n"
    "        --oslIncludePath [DIRPATH]      TODO\n"
    "        --libraries [STRING]            TODO\n"
    "        --prefix [STRING]               TODO\n"
    "        --help                          Display the complete list of command-line options\n";

template <class T> void parseToken(std::string token, std::string type, T& res)
{
    if (token.empty())
    {
        return;
    }

    mx::ValuePtr value = mx::Value::createValueFromStrings(token, type);

    if (!value)
    {
        std::cout << "Unable to parse token " << token << " as type " << type << std::endl;

        return;
    }

    res = value->asA<T>();
}

int main(int argc, char* const argv[])
{
    std::vector<std::string> tokens;

    // Gather the provided arguments.
    for (int i = 1; i < argc; i++)
    {
        tokens.emplace_back(argv[i]);
    }

    std::string argOutputPath;
    std::string argOslCompilerPath;
    std::string argOslIncludePath;
    std::string argLibraries;
    std::string argPrefix;

    // Loop over the provided arguments, and store their associated values.
    for (size_t i = 0; i < tokens.size(); i++)
    {
        const std::string& token = tokens[i];
        const std::string& nextToken = i + 1 < tokens.size() ? tokens[i + 1] : mx::EMPTY_STRING;

        if (token == "--outputPath")
        {
            argOutputPath = nextToken;
        }
        else if (token == "--oslCompilerPath")
        {
            argOslCompilerPath = nextToken;
        }
        else if (token == "--oslIncludePath")
        {
            argOslIncludePath = nextToken;
        }
        else if (token == "--libraries")
        {
            argLibraries = nextToken;
        }
        else if (token == "--prefix")
        {
            argPrefix = nextToken;
        }
        else if (token == "--help")
        {
            std::cout << "MaterialXGenOslNodes - LibsToOso version " << mx::getVersionString() << std::endl;
            std::cout << options << std::endl;

            return 0;
        }
        else
        {
            std::cout << "Unrecognized command-line option: " << token << std::endl;
            std::cout << "Launch the graph editor with '--help' for a complete list of supported "
                         "options."
                      << std::endl;

            continue;
        }

        if (nextToken.empty())
        {
            std::cout << "Expected another token following command-line option: " << token << std::endl;
        }
        else
        {
            i++;
        }
    }

    // TODO: Debug prints, to be removed.
    std::cout << "MaterialXGenOslNodes - LibsToOso" << std::endl;
    std::cout << "\toutputPath: " << argOutputPath << std::endl;
    std::cout << "\toslCompilerPath: " << argOslCompilerPath << std::endl;
    std::cout << "\toslIncludePath: " << argOslIncludePath << std::endl;
    std::cout << "\tlibraries: " << argLibraries << std::endl;
    std::cout << "\tprefix: " << argPrefix << std::endl;

    // Ensure we have a valid output path.
    mx::FilePath outputPath(argOutputPath);

    if (!outputPath.exists() || !outputPath.isDirectory())
    {
        outputPath.createDirectory();

        if (!outputPath.exists() || !outputPath.isDirectory())
        {
            std::cerr << "Failed to find and/or create the provided output "
                         "path: "
                      << outputPath.asString() << std::endl;

            return 1;
        }
    }

    // Ensure we have a valid path to the OSL compiler.
    mx::FilePath oslCompilerPath(argOslCompilerPath);

    if (!oslCompilerPath.exists())
    {
        std::cerr << "The provided path to the OSL compiler is not valid: " << oslCompilerPath.asString() << std::endl;

        return 1;
    }

    // Ensure we have a valid path to the OSL includes.
    mx::FilePath oslIncludePath(argOslIncludePath);

    if (!oslIncludePath.exists() || !oslIncludePath.isDirectory())
    {
        std::cerr << "The provided path to the OSL includes is not valid: " << oslIncludePath.asString() << std::endl;

        return 1;
    }

    // Create the libraries search path and document.
    mx::FileSearchPath librariesSearchPath = mx::getDefaultDataSearchPath();
    mx::DocumentPtr librariesDoc = mx::createDocument();

    // If a list of comma separated libraries was provided, load them individually into our document.
    if (!argLibraries.empty())
    {
        // TODO: Should we check that we actually split something based on the separator, just to be sure?
        const mx::StringVec& librariesVec = mx::splitString(argLibraries, ",");
        mx::FilePathVec librariesPaths{ "libraries/targets" };

        for (const std::string& library : librariesVec)
        {
            librariesPaths.emplace_back("libraries/" + library);
        }

        loadLibraries(librariesPaths, librariesSearchPath, librariesDoc);
    }
    // Otherwise, simply load all the available libraries.
    else
    {
        loadLibraries({ "libraries" }, librariesSearchPath, librariesDoc);
    }

    // Create and setup the `OslRenderer` that will be used to both generate the `.osl` files as well as compile
    // them to `.oso` files.
    mx::OslRendererPtr oslRenderer = mx::OslRenderer::create();
    oslRenderer->setOslCompilerExecutable(oslCompilerPath);

    // Build the list of include paths that will be passed to the `OslRenderer`.
    mx::FileSearchPath oslRendererIncludePaths;

    // Add the provided OSL include path.
    oslRendererIncludePaths.append(oslIncludePath);
    // Add the MaterialX's OSL include path.
    oslRendererIncludePaths.append(librariesSearchPath.find("libraries/stdlib/genosl/include"));

    oslRenderer->setOslIncludePath(oslRendererIncludePaths);

    // Create the OSL shader generator.
    mx::ShaderGeneratorPtr oslShaderGen = mx::OslShaderGenerator::create();

    // Register types from the libraries on the OSL shader generator.
    oslShaderGen->registerTypeDefs(librariesDoc);

    // Setup the context of the OSL shader generator.
    mx::GenContext context(oslShaderGen);
    context.getOptions().addUpstreamDependencies = false;
    context.registerSourceCodeSearchPath(librariesSearchPath);
    context.getOptions().fileTextureVerticalFlip = true;

    // TODO: Add control over the name of the log file?
    // Create a log file in the provided output path.
    const mx::FilePath& logFilePath(outputPath.asString() + "/genoslnodes_libs_to_oso.txt");
    std::ofstream logFile;

    logFile.open(logFilePath);

    // We'll use this boolean to return an error code is one of the `NodeDef` failed to codegen/compile.
    bool hasFailed = false;

    // Loop over all the `NodeDef` gathered in our documents from the provided libraries.
    for (const mx::NodeDefPtr& nodeDef : librariesDoc->getNodeDefs())
    {
        std::string nodeName = nodeDef->getName();

        // Remove the "ND_" prefix from a valid `NodeDef` name.
        if (nodeName.size() > 3 && nodeName.substr(0, 3) == "ND_")
        {
            nodeName = nodeName.substr(3);
        }

        // Add a prefix to the shader's name, both in the filename as well as inside the shader itself.
        if (!argPrefix.empty())
        {
            nodeName = argPrefix + "_" + nodeName;
        }

        // Determine whether or not there's a valid implementation of the current `NodeDef` for the type associated
        // to our OSL shader generator, i.e. OSL, and if not, skip it.
        mx::InterfaceElementPtr nodeImpl = nodeDef->getImplementation(oslShaderGen->getTarget());

        if (!nodeImpl)
        {
            logFile << "The following `NodeDef` does not provide a valid OSL implementation, "
                       "and will be skipped: "
                    << nodeName << std::endl;

            continue;
        }

        // TODO: Check for the existence/validity of the `Node`?
        mx::NodePtr node = librariesDoc->addNodeInstance(nodeDef, nodeName);
        const std::string oslFileName = nodeName + ".osl";

        try
        {
            // Codegen the `Node` to OSL.
            mx::ShaderPtr oslShader = oslShaderGen->generate(node->getName(), node, context);

            const std::string& oslFilePath = (outputPath / oslFileName).asString();
            std::ofstream oslFile;

            // TODO: Check that we have a valid/opened file descriptor before doing anything with it?
            oslFile.open(oslFilePath);
            // Dump the content of the codegen'd `NodeDef` to our `.osl` file.
            oslFile << oslShader->getSourceCode();
            oslFile.close();

            // Compile the `.osl` file to a `.oso` file next to it.
            oslRenderer->compileOSL(oslFilePath);
        }
        // Catch any codegen/compilation related exceptions.
        catch (mx::ExceptionRenderError& exc)
        {
            logFile << "Encountered a codegen/compilation related exception for the "
                       "following node: "
                    << nodeName << std::endl;
            logFile << exc.what() << std::endl;

            // Dump details about the exception in the log file.
            for (const std::string& error : exc.errorLog())
            {
                logFile << error << std::endl;
            }

            hasFailed = true;
        }
        // Catch any other exceptions
        catch (mx::Exception& exc)
        {
            logFile << "Failed to codegen/compile the following node to OSL: " << nodeName << std::endl;
            logFile << exc.what() << std::endl;

            hasFailed = true;
        }

        librariesDoc->removeChild(node->getName());
    }

    logFile.close();

    // If something went wrong, return an appropriate error code.
    if (hasFailed)
    {
        std::cerr << "Failed to codegen and compile all the OSL shaders associated to the provided MaterialX "
                     "libraries, see the log file for more details."
                  << std::endl;

        return 1;
    }

    return 0;
}
