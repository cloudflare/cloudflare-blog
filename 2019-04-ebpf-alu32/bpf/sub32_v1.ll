; ModuleID = 'bpf/sub32_v1.c'
source_filename = "bpf/sub32_v1.c"
target datalayout = "e-m:e-p:64:64-i64:64-n32:64-S128"
target triple = "bpf"

; Function Attrs: norecurse nounwind readnone
define dso_local i64 @sub32_v1(i64, i64) local_unnamed_addr #0 {
  %3 = sub i64 %0, %1
  %4 = and i64 %3, 4294967295
  ret i64 %4
}

attributes #0 = { norecurse nounwind readnone "correctly-rounded-divide-sqrt-fp-math"="false" "disable-tail-calls"="false" "less-precise-fpmad"="false" "no-frame-pointer-elim"="true" "no-frame-pointer-elim-non-leaf" "no-infs-fp-math"="false" "no-jump-tables"="false" "no-nans-fp-math"="false" "no-signed-zeros-fp-math"="false" "no-trapping-math"="false" "stack-protector-buffer-size"="8" "target-features"="+alu32" "unsafe-fp-math"="false" "use-soft-float"="false" }

!llvm.module.flags = !{!0}
!llvm.ident = !{!1}

!0 = !{i32 1, !"wchar_size", i32 4}
!1 = !{!"clang version 7.0.1 (Fedora 7.0.1-4.fc29)"}
