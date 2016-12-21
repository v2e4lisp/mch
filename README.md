mch is a simple command for text manipulation.

mch reads from the stdin line by line, matching each one against the input pattern (specified by `-i`).
If a line is fully matched it will be printed out to the stdout based on the output pattern (specified by `-o`).

In the input pattern, everything except `$` is matched as it is, while `$` matches 0 or more chars until it meets its following char.
For exmaple, the `$` in `$c` matches anything as long as it's not `c`.

In the output pattern, `$` followed by a number refers to the string that is matched by its counterpart in the input pattern.
The number works as an index starting from 1; `$0` refers to the whole line. As in the input pattern everything else is what it is, no escape is available.

mch scans lines byte by byte, that is to say in the input pattern if `c` in `$c` is a mulitbyte unicode, mch won't work in this case.


```
# Example: extract uri from nginx access log

$ echo '123.65.150.10 - - [23/Aug/2010:03:50:59 +0000] "POST /wordpress3/wp-admin/admin-ajax.php HTTP/1.1" 200 2 "http://www.example.com/wordpress3/wp-admin/post-new.php" "Mozilla/5.0 (Macintosh; U; Intel Mac OS X 10_6_4; en-US) AppleWebKit/534.3 (KHTML, like Gecko) Chrome/6.0.472.25 Safari/534.3"' | mch -i '$ $ $ [$] "$ $ $"$' -o '$6'
/wordpress3/wp-admin/admin-ajax.php

# Example: csv -> tsv (tab is entered by C-v [TAB])
$ echo 'col1,col2,col3' | mch -i '$,$,$' -o '$3	$1'
col3	col1
```
