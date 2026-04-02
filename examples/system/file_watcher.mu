print "=== file_watcher.mu ==="
print "Watching file \"target.txt\" in current directory every 2 seconds."
print "Stop by killing the process (Ctrl+C).\n"

var watched_file = "target.txt"

proc watch_once() {
    var st = filestat(watched_file)
    var exists = st[0]
    if (exists) {
        var size = st[1]
        var perms = st[3]
        print "[watch] file exists: " watched_file ", size = " size ", perms = " perms
    } else {
        print "[watch] file does not exist: " watched_file
    }
}

while (1) {
    watch_once()
    sleep(2000)
}
