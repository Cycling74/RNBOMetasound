// based on Copyright Epic Games, Inc. All Rights Reserved.

using UnrealBuildTool;
using EpicGames.Core;
using System;
using System.IO;
using System.Globalization;
using System.Text;
using System.Text.RegularExpressions;
using System.Collections.Generic;


public class RNBOMetasound : ModuleRules
{
	string OperatorTemplate { get; set; }

	public RNBOMetasound(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = ModuleRules.PCHUsageMode.UseExplicitOrSharedPCHs;

		UnsafeTypeCastWarningLevel = WarningLevel.Warning;
		ShadowVariableWarningLevel = WarningLevel.Warning;

		var templateDir = Path.Combine(PluginDirectory, "Source", "RNBOMetasound", "Template");
		var templateFile = Path.Combine(templateDir, "MetaSoundOperator.cpp.in");
		var templateHeaderFile = Path.Combine(templateDir, "MetaSoundOperator.h.in");
		using (StreamReader streamReader = new StreamReader(templateFile, Encoding.UTF8))
		{
			OperatorTemplate = streamReader.ReadToEnd();
		}

		var exportDir = Path.Combine(PluginDirectory, "Exports");
        if (!Directory.Exists(exportDir)) {
            throw new InvalidOperationException("RNBOMetasound cannot build without at least one export in the Exports directory");
        }

		string rnboDir = null;

		using (StreamWriter writer = new StreamWriter(Path.Combine(PluginDirectory, "Source", "RNBOMetasound", "Private", "RNBOMetasoundGenerated.cpp")))
		{
			writer.WriteLine("//automatically generated by RNBOMetasound");

			using (StreamReader streamReader = new StreamReader(templateHeaderFile, Encoding.UTF8))
			{
				writer.WriteLine(streamReader.ReadToEnd());
			}

			//detect exports, add includes and generate metasounds
			foreach (var path in Directory.GetDirectories(exportDir)) {
				//include export dir
				PrivateIncludePaths.Add(path);
				//set rnbo dir
				if (rnboDir == null) {
                    var p = Path.Combine(path, "rnbo");
                    if (Directory.Exists(p)) {
                        rnboDir = p;
                    }
				}

				//#include cpp files in export dir
				foreach (var f in Directory.GetFiles(path, "*.cpp")) {
					writer.WriteLine("#include \"{0}/{1}\" ", new DirectoryInfo(path).Name, Path.GetFileName(f));
				}
				writer.Write(CreateMetaSound(path));
			}
		}

        if (rnboDir == null) {
            throw new InvalidOperationException("RNBOMetasound cannot build without the rnbo/ source directory in at least one of the export directories");
        }

		ExternalDependencies.Add(templateFile);
		ExternalDependencies.Add(exportDir);

		PublicIncludePaths.AddRange(
			new string[] {
				// ... add public include paths required here ...
			}
			);


		PrivateIncludePaths.AddRange(
			new string[]
			{
				exportDir,
				Path.Combine(rnboDir),
				Path.Combine(rnboDir, "common"),
				Path.Combine(rnboDir, "src"),
				Path.Combine(rnboDir, "src", "3rdparty"),
			}
			);


		PublicDependencyModuleNames.AddRange(
			new string[]
			{
				"Core",
				"MetasoundFrontend",
				"MetasoundGraphCore",
				"MetasoundStandardNodes",
				"MetasoundEngine",
				"Harmonix",
				"HarmonixMetasound",
				"HarmonixMidi",
				// ... add other public dependencies that you statically link with here ...
			}
			);


		PrivateDependencyModuleNames.AddRange(
			new string[]
			{
				"CoreUObject",
				"Engine",
				"SignalProcessing",
				// ... add private dependencies that you statically link with here ...
			}
			);


		DynamicallyLoadedModuleNames.AddRange(
			new string[]
			{
				// ... add any modules that your module loads dynamically here ...
			}
			);

		PrivateDefinitions.Add("RNBO_NO_PATCHERFACTORY=1");
	}

	string CreateMetaSound(string path) {
		var descPath = Path.Combine(path, "description.json");
        string descString = File.ReadAllText(descPath);

        //get the name
		JsonObject desc = JsonObject.Parse(descString);
        var meta = desc.GetObjectField("meta");
		string name = meta.GetStringField("rnboobjname");

		return OperatorTemplate
			.Replace("_OPERATOR_NAME_", name)
            //TODO chunk for windows
			.Replace("_OPERATOR_DESC_", String.Format("R\"RNBOLIT({0})RNBOLIT\"", descString))
			;
	}
}
