Training Sceadan
================

This document describes the procedure for training Sceadan and
building a custom classifier:

1. Sceadan is a C++ program that uses liblinear. You will need the following tools and libraries installed:

   c++ automake autoconf liblinear libm git-all

   On Fedora you can install these with:

   $ sudo yum install gcc-c++ automake autoconf liblinear liblinear-cli liblinear-doc

1. Download sceadan if you don't have it:

   $ git clone --recursive git@github.com:nbeebe/sceadan.git
   $ cd sceadan

2. Run bootstrap to make configure if you don't have it. Then run configure:

   $ sh bootstrap.sh
   $ ./configure
   $ make

3. Make a list of the file types that you wish to classify.

4. Collect at least 40 samples of each file type, where the average file size is 100K - 1MB.

   [THIS DOESN'T WORK YET:]
   Ideally, the files should be from the country of interest.  If you
   intend to analyze data from multiple languages, create sub file
   types with the ISO 639-1 language code. e.g. doc-EN, doc-ES, doc-AR  
   (http://www.loc.gov/standards/iso639-2/php/code_list.php)

5. Place the files in a directory hiearchy DATA/TYPE/file  where TYPE
   is the code above (e..g doc-EN, JPG, etc.)

   DATA/<filetype>/document1
   DATA/<filetype>/document2

   e.g.:

   DATA/doc/filename.doc
   DATA/doc/any_name_is_kay.doc
   DATA/docx/another_file.docx
   DATA/docx/yet_another_file.docx
   ...

6. cd tools

7. Validate the training data with the sceadan_train.py program. This
   program is in Python3 and must be run with a python3 interpreter, as
   shown below ($ is the Linux prompt):

   $ python3 tools/sceadan_train.py --validate --data=DATA/ 

   --validate   --- Validate what's in DATA
   --data=      --- specifies data repository

8. Try to train with 50% of the data for training and 50% for testing:

   $ python3 tools/sceadan_train.py --data=DATA/  --exp=experiment1

   --data=      --- specifies data repository
   --exp=       --- specifies where experiment gets put


