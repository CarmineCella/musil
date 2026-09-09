# sub/c.mu — resolved relative to b.mu, not to the current directory
var loads (append loads "c")
load "../b.mu"
function from-c () "c"
