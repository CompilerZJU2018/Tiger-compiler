for file in ./testcases/*.tig
do
  echo "Compiling ${file}... "
  ./a.out $file 2>&1
done