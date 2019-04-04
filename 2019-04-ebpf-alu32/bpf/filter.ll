; ModuleID = 'bpf/filter.c'
source_filename = "bpf/filter.c"
target datalayout = "e-m:e-p:64:64-i64:64-n32:64-S128"
target triple = "bpf"

%struct.bpf_map_def = type { i32, i32, i32, i32, i32 }
%struct.__sk_buff = type { i32, i32, i32, i32, i32, i32, i32, i32, i32, i32, i32, i32, [5 x i32], i32, i32, i32, i32, i32, i32, i32, i32, [4 x i32], [4 x i32], i32, i32, i32, %union.anon }
%union.anon = type { %struct.bpf_flow_keys* }
%struct.bpf_flow_keys = type { i16, i16, i16, i8, i8, i8, i8, i16, i16, i16, %union.anon.0 }
%union.anon.0 = type { %struct.anon.1 }
%struct.anon.1 = type { [4 x i32], [4 x i32] }

@args = dso_local global %struct.bpf_map_def { i32 2, i32 4, i32 8, i32 3, i32 0 }, section "maps", align 4
@__license = dso_local global [13 x i8] c"Dual BSD/GPL\00", section "license", align 1
@llvm.used = appending global [6 x i8*] [i8* getelementptr inbounds ([13 x i8], [13 x i8]* @__license, i32 0, i32 0), i8* bitcast (%struct.bpf_map_def* @args to i8*), i8* bitcast (i32 (%struct.__sk_buff*)* @filter_alu32 to i8*), i8* bitcast (i32 (%struct.__sk_buff*)* @filter_alu64 to i8*), i8* bitcast (i32 (%struct.__sk_buff*)* @filter_ir to i8*), i8* bitcast (i32 (%struct.__sk_buff*)* @filter_stv to i8*)], section "llvm.metadata"

; Function Attrs: nounwind
define dso_local i32 @filter_alu64(%struct.__sk_buff* nocapture readnone) #0 section "socket1" {
  %2 = alloca i32, align 4
  %3 = alloca i64, align 8
  %4 = alloca i32, align 4
  %5 = alloca i32, align 4
  %6 = bitcast i32* %5 to i8*
  call void @llvm.lifetime.start.p0i8(i64 4, i8* nonnull %6)
  store i32 0, i32* %5, align 4, !tbaa !2
  %7 = call i8* inttoptr (i64 1 to i8* (i8*, i8*)*)(i8* bitcast (%struct.bpf_map_def* @args to i8*), i8* nonnull %6) #3
  %8 = icmp eq i8* %7, null
  br i1 %8, label %12, label %9

; <label>:9:                                      ; preds = %1
  %10 = bitcast i8* %7 to i64*
  %11 = load i64, i64* %10, align 8, !tbaa !6
  br label %12

; <label>:12:                                     ; preds = %1, %9
  %13 = phi i64 [ %11, %9 ], [ 0, %1 ]
  call void @llvm.lifetime.end.p0i8(i64 4, i8* nonnull %6)
  %14 = bitcast i32* %4 to i8*
  call void @llvm.lifetime.start.p0i8(i64 4, i8* nonnull %14)
  store i32 1, i32* %4, align 4, !tbaa !2
  %15 = call i8* inttoptr (i64 1 to i8* (i8*, i8*)*)(i8* bitcast (%struct.bpf_map_def* @args to i8*), i8* nonnull %14) #3
  %16 = icmp eq i8* %15, null
  br i1 %16, label %20, label %17

; <label>:17:                                     ; preds = %12
  %18 = bitcast i8* %15 to i64*
  %19 = load i64, i64* %18, align 8, !tbaa !6
  br label %20

; <label>:20:                                     ; preds = %12, %17
  %21 = phi i64 [ %19, %17 ], [ 0, %12 ]
  call void @llvm.lifetime.end.p0i8(i64 4, i8* nonnull %14)
  %22 = sub i64 %13, %21
  %23 = bitcast i32* %2 to i8*
  call void @llvm.lifetime.start.p0i8(i64 4, i8* nonnull %23)
  %24 = bitcast i64* %3 to i8*
  call void @llvm.lifetime.start.p0i8(i64 8, i8* nonnull %24)
  store i32 2, i32* %2, align 4, !tbaa !2
  store i64 %22, i64* %3, align 8, !tbaa !6
  %25 = call i8* inttoptr (i64 2 to i8* (i8*, i8*, i8*, i64)*)(i8* bitcast (%struct.bpf_map_def* @args to i8*), i8* nonnull %23, i8* nonnull %24, i64 0) #3
  call void @llvm.lifetime.end.p0i8(i64 4, i8* nonnull %23)
  call void @llvm.lifetime.end.p0i8(i64 8, i8* nonnull %24)
  ret i32 1
}

; Function Attrs: argmemonly nounwind
declare void @llvm.lifetime.start.p0i8(i64, i8* nocapture) #1

; Function Attrs: argmemonly nounwind
declare void @llvm.lifetime.end.p0i8(i64, i8* nocapture) #1

; Function Attrs: nounwind
define dso_local i32 @filter_alu32(%struct.__sk_buff* nocapture readnone) #0 section "socket2" {
  %2 = alloca i32, align 4
  %3 = alloca i64, align 8
  %4 = alloca i32, align 4
  %5 = alloca i32, align 4
  %6 = bitcast i32* %5 to i8*
  call void @llvm.lifetime.start.p0i8(i64 4, i8* nonnull %6)
  store i32 0, i32* %5, align 4, !tbaa !2
  %7 = call i8* inttoptr (i64 1 to i8* (i8*, i8*)*)(i8* bitcast (%struct.bpf_map_def* @args to i8*), i8* nonnull %6) #3
  %8 = icmp eq i8* %7, null
  br i1 %8, label %12, label %9

; <label>:9:                                      ; preds = %1
  %10 = bitcast i8* %7 to i64*
  %11 = load i64, i64* %10, align 8, !tbaa !6
  br label %12

; <label>:12:                                     ; preds = %1, %9
  %13 = phi i64 [ %11, %9 ], [ 0, %1 ]
  call void @llvm.lifetime.end.p0i8(i64 4, i8* nonnull %6)
  %14 = bitcast i32* %4 to i8*
  call void @llvm.lifetime.start.p0i8(i64 4, i8* nonnull %14)
  store i32 1, i32* %4, align 4, !tbaa !2
  %15 = call i8* inttoptr (i64 1 to i8* (i8*, i8*)*)(i8* bitcast (%struct.bpf_map_def* @args to i8*), i8* nonnull %14) #3
  %16 = icmp eq i8* %15, null
  br i1 %16, label %20, label %17

; <label>:17:                                     ; preds = %12
  %18 = bitcast i8* %15 to i64*
  %19 = load i64, i64* %18, align 8, !tbaa !6
  br label %20

; <label>:20:                                     ; preds = %12, %17
  %21 = phi i64 [ %19, %17 ], [ 0, %12 ]
  call void @llvm.lifetime.end.p0i8(i64 4, i8* nonnull %14)
  %22 = trunc i64 %13 to i32
  %23 = trunc i64 %21 to i32
  %24 = sub i64 %13, %21
  %25 = lshr i64 %13, 32
  %26 = lshr i64 %21, 32
  %27 = sub nsw i64 %25, %26
  %28 = icmp ult i32 %22, %23
  %29 = sext i1 %28 to i64
  %30 = add nsw i64 %27, %29
  %31 = shl i64 %30, 32
  %32 = and i64 %24, 4294967295
  %33 = or i64 %31, %32
  %34 = bitcast i32* %2 to i8*
  call void @llvm.lifetime.start.p0i8(i64 4, i8* nonnull %34)
  %35 = bitcast i64* %3 to i8*
  call void @llvm.lifetime.start.p0i8(i64 8, i8* nonnull %35)
  store i32 2, i32* %2, align 4, !tbaa !2
  store i64 %33, i64* %3, align 8, !tbaa !6
  %36 = call i8* inttoptr (i64 2 to i8* (i8*, i8*, i8*, i64)*)(i8* bitcast (%struct.bpf_map_def* @args to i8*), i8* nonnull %34, i8* nonnull %35, i64 0) #3
  call void @llvm.lifetime.end.p0i8(i64 4, i8* nonnull %34)
  call void @llvm.lifetime.end.p0i8(i64 8, i8* nonnull %35)
  ret i32 1
}

; Function Attrs: nounwind
define dso_local i32 @filter_ir(%struct.__sk_buff* nocapture readnone) #0 section "socket3" {
  %2 = alloca i32, align 4
  %3 = alloca i64, align 8
  %4 = alloca i32, align 4
  %5 = alloca i32, align 4
  %6 = bitcast i32* %5 to i8*
  call void @llvm.lifetime.start.p0i8(i64 4, i8* nonnull %6)
  store i32 0, i32* %5, align 4, !tbaa !2
  %7 = call i8* inttoptr (i64 1 to i8* (i8*, i8*)*)(i8* bitcast (%struct.bpf_map_def* @args to i8*), i8* nonnull %6) #3
  %8 = icmp eq i8* %7, null
  br i1 %8, label %12, label %9

; <label>:9:                                      ; preds = %1
  %10 = bitcast i8* %7 to i64*
  %11 = load i64, i64* %10, align 8, !tbaa !6
  br label %12

; <label>:12:                                     ; preds = %1, %9
  %13 = phi i64 [ %11, %9 ], [ 0, %1 ]
  call void @llvm.lifetime.end.p0i8(i64 4, i8* nonnull %6)
  %14 = bitcast i32* %4 to i8*
  call void @llvm.lifetime.start.p0i8(i64 4, i8* nonnull %14)
  store i32 1, i32* %4, align 4, !tbaa !2
  %15 = call i8* inttoptr (i64 1 to i8* (i8*, i8*)*)(i8* bitcast (%struct.bpf_map_def* @args to i8*), i8* nonnull %14) #3
  %16 = icmp eq i8* %15, null
  br i1 %16, label %20, label %17

; <label>:17:                                     ; preds = %12
  %18 = bitcast i8* %15 to i64*
  %19 = load i64, i64* %18, align 8, !tbaa !6
  br label %20

; <label>:20:                                     ; preds = %12, %17
  %21 = phi i64 [ %19, %17 ], [ 0, %12 ]
  call void @llvm.lifetime.end.p0i8(i64 4, i8* nonnull %14)
  %22 = call i64 @sub64_ir(i64 %13, i64 %21) #3
  %23 = bitcast i32* %2 to i8*
  call void @llvm.lifetime.start.p0i8(i64 4, i8* nonnull %23)
  %24 = bitcast i64* %3 to i8*
  call void @llvm.lifetime.start.p0i8(i64 8, i8* nonnull %24)
  store i32 2, i32* %2, align 4, !tbaa !2
  store i64 %22, i64* %3, align 8, !tbaa !6
  %25 = call i8* inttoptr (i64 2 to i8* (i8*, i8*, i8*, i64)*)(i8* bitcast (%struct.bpf_map_def* @args to i8*), i8* nonnull %23, i8* nonnull %24, i64 0) #3
  call void @llvm.lifetime.end.p0i8(i64 4, i8* nonnull %23)
  call void @llvm.lifetime.end.p0i8(i64 8, i8* nonnull %24)
  ret i32 1
}

declare dso_local i64 @sub64_ir(i64, i64) local_unnamed_addr #2

; Function Attrs: nounwind
define dso_local i32 @filter_stv(%struct.__sk_buff* nocapture readnone) #0 section "socket4" {
  %2 = alloca i32, align 4
  %3 = alloca i64, align 8
  %4 = alloca i32, align 4
  %5 = alloca i32, align 4
  %6 = alloca i32, align 4
  %7 = alloca i32, align 4
  %8 = bitcast i32* %7 to i8*
  call void @llvm.lifetime.start.p0i8(i64 4, i8* nonnull %8)
  store i32 0, i32* %7, align 4, !tbaa !2
  %9 = call i8* inttoptr (i64 1 to i8* (i8*, i8*)*)(i8* bitcast (%struct.bpf_map_def* @args to i8*), i8* nonnull %8) #3
  %10 = icmp eq i8* %9, null
  br i1 %10, label %14, label %11

; <label>:11:                                     ; preds = %1
  %12 = bitcast i8* %9 to i64*
  %13 = load i64, i64* %12, align 8, !tbaa !6
  br label %14

; <label>:14:                                     ; preds = %1, %11
  %15 = phi i64 [ %13, %11 ], [ 0, %1 ]
  call void @llvm.lifetime.end.p0i8(i64 4, i8* nonnull %8)
  %16 = bitcast i32* %6 to i8*
  call void @llvm.lifetime.start.p0i8(i64 4, i8* nonnull %16)
  store i32 1, i32* %6, align 4, !tbaa !2
  %17 = call i8* inttoptr (i64 1 to i8* (i8*, i8*)*)(i8* bitcast (%struct.bpf_map_def* @args to i8*), i8* nonnull %16) #3
  %18 = icmp eq i8* %17, null
  br i1 %18, label %22, label %19

; <label>:19:                                     ; preds = %14
  %20 = bitcast i8* %17 to i64*
  %21 = load i64, i64* %20, align 8, !tbaa !6
  br label %22

; <label>:22:                                     ; preds = %14, %19
  %23 = phi i64 [ %21, %19 ], [ 0, %14 ]
  call void @llvm.lifetime.end.p0i8(i64 4, i8* nonnull %16)
  %24 = bitcast i32* %4 to i8*
  call void @llvm.lifetime.start.p0i8(i64 4, i8* nonnull %24)
  %25 = bitcast i32* %5 to i8*
  call void @llvm.lifetime.start.p0i8(i64 4, i8* nonnull %25)
  %26 = trunc i64 %15 to i32
  %27 = trunc i64 %23 to i32
  %28 = sub i32 %26, %27
  store volatile i32 %28, i32* %5, align 4, !tbaa !2
  %29 = lshr i64 %15, 32
  %30 = trunc i64 %29 to i32
  %31 = lshr i64 %23, 32
  %32 = trunc i64 %31 to i32
  %33 = sub i32 %30, %32
  %34 = icmp ult i32 %26, %27
  %35 = sext i1 %34 to i32
  %36 = add i32 %33, %35
  store volatile i32 %36, i32* %4, align 4, !tbaa !2
  %37 = zext i32 %36 to i64
  %38 = shl nuw i64 %37, 32
  %39 = zext i32 %28 to i64
  %40 = or i64 %38, %39
  call void @llvm.lifetime.end.p0i8(i64 4, i8* nonnull %25)
  call void @llvm.lifetime.end.p0i8(i64 4, i8* nonnull %24)
  %41 = bitcast i32* %2 to i8*
  call void @llvm.lifetime.start.p0i8(i64 4, i8* nonnull %41)
  %42 = bitcast i64* %3 to i8*
  call void @llvm.lifetime.start.p0i8(i64 8, i8* nonnull %42)
  store i32 2, i32* %2, align 4, !tbaa !2
  store i64 %40, i64* %3, align 8, !tbaa !6
  %43 = call i8* inttoptr (i64 2 to i8* (i8*, i8*, i8*, i64)*)(i8* bitcast (%struct.bpf_map_def* @args to i8*), i8* nonnull %41, i8* nonnull %42, i64 0) #3
  call void @llvm.lifetime.end.p0i8(i64 4, i8* nonnull %41)
  call void @llvm.lifetime.end.p0i8(i64 8, i8* nonnull %42)
  ret i32 1
}

attributes #0 = { nounwind "correctly-rounded-divide-sqrt-fp-math"="false" "disable-tail-calls"="false" "less-precise-fpmad"="false" "no-frame-pointer-elim"="true" "no-frame-pointer-elim-non-leaf" "no-infs-fp-math"="false" "no-jump-tables"="false" "no-nans-fp-math"="false" "no-signed-zeros-fp-math"="false" "no-trapping-math"="false" "stack-protector-buffer-size"="8" "target-features"="+alu32" "unsafe-fp-math"="false" "use-soft-float"="false" }
attributes #1 = { argmemonly nounwind }
attributes #2 = { "correctly-rounded-divide-sqrt-fp-math"="false" "disable-tail-calls"="false" "less-precise-fpmad"="false" "no-frame-pointer-elim"="true" "no-frame-pointer-elim-non-leaf" "no-infs-fp-math"="false" "no-nans-fp-math"="false" "no-signed-zeros-fp-math"="false" "no-trapping-math"="false" "stack-protector-buffer-size"="8" "target-features"="+alu32" "unsafe-fp-math"="false" "use-soft-float"="false" }
attributes #3 = { nounwind }

!llvm.module.flags = !{!0}
!llvm.ident = !{!1}

!0 = !{i32 1, !"wchar_size", i32 4}
!1 = !{!"clang version 7.0.1 (Fedora 7.0.1-4.fc29)"}
!2 = !{!3, !3, i64 0}
!3 = !{!"int", !4, i64 0}
!4 = !{!"omnipotent char", !5, i64 0}
!5 = !{!"Simple C/C++ TBAA"}
!6 = !{!7, !7, i64 0}
!7 = !{!"long", !4, i64 0}
