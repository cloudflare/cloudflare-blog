set debuginfod enabled off
break fib
commands
  silent
  printf "n = %d, %%rsp = %#x\n", n, $rsp
  continue
end

run
