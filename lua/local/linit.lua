
rex.new = rex.newPOSIX

function rex.grep(filename, expr)
	if not posix.stat(filename, "mode") then
		return nil
	end
	local lines = {}
	local pat = rex.new(expr)
	local pos = 1
	for line in io.lines(filename) do
		if pat:match(line) then
			table.insert(lines, pos, line)
		end
		pos = pos + 1
	end
	if table.getn(lines) == 0 then
		return nil
	end
	return lines
end

function rex.igrep(filename, expr)
	return ipairs(rex.grep(filename, expr))
end

function rex.bgrep(filename, expr)
	if not posix.stat(filename, "mode") then
		return nil
	end
	local pat = rex.new(expr)
	for line in io.lines(filename) do
		if pat:match(line) then
			return true
		end
	end
	return false
end

