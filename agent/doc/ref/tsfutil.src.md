## tsfutil

TS File utility - tool to convert TSF files from/to JSON or CSV.

Usage:
`$ tsfutil [global options] command [options] <tsffile> [infile|outfile]`

Where global options are:
    * `-s schema.json`	
		Path to file that contains TSF schema
    * `-F json|jsonraw|csv` 
		Input/output tsfutil format - default is json

Subcommand is one of:
    * __create__
		creates new TSF file from specified schema
    * __count__
		prints number of entries in TSF file
    * __get__
		read entries from tsffile and write them to outfile or stdin
		if it is not specified. By default reads all entries from file.
		To specify desired range of entries, specify option -g:
          * -g N[:M|$]
		    	Where N - start entry, M - last entry, and $ - alias for last
		    	entry in tsffile. 
    * __add__
		adds entries from infile to tsffile

Also, one of backend options may be specified using -o option:
	-o <backend option>

JSON options:
	* one - if getting only one entry, print it alone, not array of single nodes

CSV options:
    * _header=<header>_ - additional CSV header if not specified in CSV file (for 'add' command) or header that would be used during export for 'get' command. You may specify not all columns to create tsffile slice. Also through this option you may provide header hints. For boolean columns you may change default true and false literals:
    	`-o header=somefield:yes:no`
    For integer values you may append "unsigned" or "hex" hint:
    	`-o header=int1:unsigned,flags:hex` 
    * _noheader_ - if specified for 'get' subcommand, tsfutil don't write header into file. If specified for 'add' subcommand, means that CSV file do not contain valid header and header, specified by 'header=' option should be used.
    * _all_ - for 'get' subcommand - retrieve all fields from tsffile, not just specified in 'header=' option
    * _valsep_ - separator for columns. Default is comma
    * _optsep_ - separator for column hints inside header. Default is semicolon.	

