local function dump(o, indent, recurse)
	if indent == nil then
		indent = 1
	end
	if recurse == nil then
		recurse = {}
	else
		if recurse[o] ~= nil then
			return "(recursion halted)"
		end

		local newrecurse = {}
		for i, _ in pairs(recurse) do
			newrecurse[i] = true
		end
		recurse = newrecurse
		recurse[o] = true
	end

	if type(o) == "table" then
		local s = "{\n"
		for k, v in pairs(o) do
			s = s .. string.rep("    ", indent) .. "[" .. dump(k, indent + 1, recurse) .. "] = " .. dump(v, indent + 1, recurse) .. ",\n"
		end
		return s .. string.rep("    ", indent - 1) .. "}"
	elseif type(o) == "string" then
		return "\"" .. tostring(o) .. "\""
	else
		return tostring(o)
	end
end

local function ident(name)
	-- Replace invalid characters with underscores
	name = string.gsub(name, "[^%a_]", "_")
	
	local fc = string.byte(string.sub(name, 1, 1))
	if fc >= 0x30 and fc <= 0x39 then
		name = "_"..name
	end
	
	return name
end

local function escapestring(str)
	return str:gsub('[%z\1-\31\128-\255\134\\"]', function(c)
		local num = string.byte(c)
		return `\\{(num // 64) % 8}{(num // 8) % 8}{(num // 1) % 8}`
	end)
end

return {

SourceProcess = function(source, types, enums, classes, functions)
	local name = ident(source)

	local result = "#include <"..source..">\n"
	result ..= "#include <string>\n"
	result ..= "#include <iostream>\n"

	local dumper = dump(types).."\\n"..dump(enums).."\\n"..dump(classes).."\\n"..dump(functions)
	result ..= "\nvoid "..name.."_register()\n{\n"
	for line in string.gmatch(dumper, "[^\n]+") do
		result ..= "std::cout << \"" .. escapestring(line) .. "\" << std::endl;"
	end
	result ..= "}\n"


	return result
end;

GlueProcess = function(sources)
	local result = ""

	for _, v in ipairs(sources) do
		result ..= "extern void "..ident(v.source).."_register();\n"
	end
	result ..= "\nvoid glue_register()\n{\n"
	for _, v in ipairs(sources) do
		result ..= "\t"..ident(v.source).."_register();\n"
	end
	result ..= "}\n"

	return result
end;

};
	