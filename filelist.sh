#! /bin/bash
#
# Automatic resource file list generation
# Created by Dimok

outFile="./src/filelist.h"
count_old=$(cat $outFile 2>/dev/null | tr -d '\n\n' | sed 's/[^0-9]*\([0-9]*\).*/\1/')

count=0
if [[ $OSTYPE == darwin* ]];
then

for i in $(gfind ./data/ -maxdepth 1 -type f  \( ! -printf "%f\n" \) | sort -f)
do
	files[count]=$i
	count=$((count+1))
done

else

for i in $(find ./data/ -maxdepth 1 -type f  \( ! -printf "%f\n" \) | sort -f)
do
	files[count]=$i
	count=$((count+1))
done

fi

if [ "$count_old" != "$count" ] || [ ! -f $outFile ]
then

echo "Generating filelist.h for $count files." >&2
cat <<EOF > $outFile
/****************************************************************************
 * This file is generated automatically.
 * Includes $count files.
 *
 * NOTE:
 * Any manual modification of this file will be overwriten by the generation.
 ****************************************************************************/
#ifndef _FILELIST_H_
#define _FILELIST_H_

EOF

for i in ${files[@]}
do
	filename=${i%.*}
	extension=${i##*.}
	echo 'extern const unsigned char '$filename'_'$extension'[];' >> $outFile
	echo 'extern const unsigned int '$filename'_'$extension'_size;' >> $outFile
	echo '' >> $outFile
done

echo '#endif' >> $outFile

fi
