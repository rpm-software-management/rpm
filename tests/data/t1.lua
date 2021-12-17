
print("hello world")
print("opts:")
for i,o in pairs(opt) do
    print(i, o)
end
print("args:")
for i = 1, #arg do
    print(arg[i])
end
