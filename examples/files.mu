# file I/O examples

load "system.mu"
var tmpfile "/tmp/musil_test.txt"

write tmpfile "line one\nline two\nline three\n"
print "wrote to" tmpfile

var content (read tmpfile)
print "content length:" (length content)

var nl (find content "\n")
print "first line:" (slice content 0 nl)
append-file tmpfile "line four\n"

var content2 (read tmpfile)
print "after append, length:" (length content2)
print "lines:" (file-lines tmpfile)
print "size on disk:" (file-size tmpfile) "bytes"

function log-entry (file msg) (append-file file (concat msg "\n"))
var logfile "/tmp/musil_log.txt"
write logfile ""
log-entry logfile "session started"
log-entry logfile "processing data"
log-entry logfile "done"

print "log:"
print (read logfile)

# CSV round trip: a table is a list of rows
var table (list (list "name" "midi" "hz") (list "A4" 69 440) (list "C4" 60 261.63))
write-csv "/tmp/musil_table.csv" table
print "csv back:" (read-csv "/tmp/musil_table.csv")

# directory listing
print "/tmp has" (length (ls "/tmp")) "entries"
remove tmpfile
remove logfile
remove "/tmp/musil_table.csv"
