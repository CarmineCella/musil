# file I/O examples

var tmpfile = "/tmp/musil_test.txt"

write(tmpfile, "line one\nline two\nline three\n")
print "wrote to " tmpfile

var content = read(tmpfile)
print "content length: " len(content)

var nl = find(content, "\n")
var first_line = sub(content, 0, nl)
print "first line: " first_line
append(tmpfile, "line four\n")        # this is on its own line — no longer consumed by print above

var content2 = read(tmpfile)
print "after append, length: " len(content2)

proc log_entry (file, msg) {
    append(file, msg + "\n")
}
var logfile = "/tmp/musil_log.txt"
write(logfile, "")
log_entry(logfile, "session started")
log_entry(logfile, "processing data")
log_entry(logfile, "done")

var log = read(logfile)
print "log:"
print log
