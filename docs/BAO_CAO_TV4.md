# Báo cáo phần việc TV4

Cập nhật: 21/11/2026. Phạm vi: công cụ TV4 chạy độc lập trong repo hiện tại.

## Đã hoàn thành và chạy kiểm chứng

| Hạng mục | Kết quả |
|---|---|
| Validator C++17 | Kiểm tra instance, start/goal, move/wait, vật cản, vertex/edge conflict, stay-at-target; SOC và makespan |
| Constraint validation | Vertex/edge constraint, kể cả goal constraint ở thời điểm tương lai |
| Joint-state oracle | Dijkstra tối ưu SOC, finished bits, rời goal/quay lại, dựng paths, kiểm tra lại bằng validator |
| Giới hạn oracle | Timeout, max records, max agents; phân biệt solved/infeasible/timeout/resource_limit/invalid_input/error |
| Independent LB | BFS riêng từng agent, cộng khoảng cách; nhận biết goal không reachable |
| Gap | Exact gap có optimum, upper bound khi chỉ có LB, null khi mẫu số bằng 0 |
| CLI và I/O | Đọc instance/paths, xuất JSON, lưu paths; runtime, expanded/generated, peak memory trên Unix |
| Audit solution | Kiểm tra paths và cost solver báo, đối chiếu oracle/LB, kiểm tra cận w khi đủ certificate |
| Bypass | Hook đủ điều kiện chi phí/constraints/conflict, demo và unit tests; chưa nối CT CBS |
| Thống kê | Success rate, median/IQR, exact McNemar, Wilcoxon, bootstrap CI, Holm cho hai phép thử |
| Batch/checkpoint | Runner oracle tuần tự, resume, fingerprint dữ liệu/binary/config, metadata máy |
| MovingAI adapter | Map + k dòng đầu scenario thành instance 4 hướng; kiểm tra tên/dimensions/toạ độ |
| Tài liệu | README tiếng Việt, phân tích correctness/complexity, hướng dẫn tích hợp, dàn ý video 5.5 phút |

Python chỉ dùng standard library. Chưa chép mã nguồn từ repo EECBS hay thư viện solver khác.

## Kiểm chứng thực tế

Build bằng GCC 13.3 và CMake 3.28.3; toolchain giải nén tại `/tmp/mapf-toolchain`,
không cài hay sửa package hệ thống. Lệnh `bash scripts/build.sh` build và chạy CTest.

- 3/3 nhóm CTest pass: `tv4_tests`, `python_tests`, `oracle_tests`.
- 8 Python test methods pass: CLI, path enumeration, audit/gap, checkpoint,
  exact p-values, thống kê sample, timeout/invalid, MovingAI converter.
- 64 cặp start/goal cho một agent trên map có vật cản: oracle trùng BFS.
- 144 cặp start/goal cho hai agent trên grid 2×2: oracle trùng nghiệm nhỏ nhất
  từ cách liệt kê paths từng agent độc lập rồi ghép cặp kiểm tra conflict.
- C++ tests bao gồm 3 agent, future goal/edge constraint, vô nghiệm, timeout,
  resource limit, rời goal rồi quay lại, SOC=0, Bypass accept/reject.
- Quy trình CLI → JSON/paths → validator → audit → batch → thống kê chạy được.
- Chạy thêm oracle/analysis/Bypass tests với AddressSanitizer + UndefinedBehaviorSanitizer:
  pass, không phát hiện lỗi; leak detection tắt trong lần chạy này.

| Instance/case | Kết quả xác nhận |
|---|---|
| `data/tiny.mapf` | Solved, SOC*=4, makespan=2, independent LB=4 |
| `data/leave_goal.mapf` | Solved, SOC*=4; agent ở goal tạm đi xuống alcove rồi quay lại |
| `data/swap.mapf` | Infeasible trên corridor 1×2 |
| Hai agent đổi chỗ trên 2×2 | SOC*=4 nhờ đi vòng |
| Ba agent đi ngang ba hàng | SOC*=6 |
| Bypass demo | Cùng chi phí từng agent, conflicts từ 1 xuống 0 |
| Thống kê synthetic | Được đọc/xử lý đúng; không đại diện kết quả benchmark |

## Phần cần mã và dữ liệu của nhóm

1. Repo hiện chưa có CBS/ECBS của TV1/TV2. Chưa thể tuyên bố các solver đó đúng,
   đạt cận w, hoặc có tốc độ/chất lượng cụ thể; audit sẵn sàng nhận output theo schema README.
2. Hook Bypass chưa tích hợp CT/queue/low-level của TV1; chưa có ablation Bypass on/off.
   TV1 cần cung cấp candidate và predicate kiểm tra mọi constraint của parent.
3. Chưa có dataset benchmark chính và CSV thật để hoàn tất E4/E6 hay report thực nghiệm.
   Các file `results_example.csv` chỉ minh họa, không dùng làm số liệu nộp bài.
4. Chưa có frontier CBS/ECBS hợp lệ để chứng nhận frontier LB. Đang dùng BFS LB và oracle
   optimum, không gắn nhãn path cost suboptimal là lower bound.
5. Chưa có PDF báo cáo nhóm/video quay thật. Có phần phân tích và dàn ý để ghép vào sản phẩm nhóm.

## Cách chạy ngắn nhất

```bash
cd /home/khoa/dev/multi_agent_path
bash scripts/build.sh
./build/tv4_demo
./build/tv4_cli --mode oracle --instance data/tiny.mapf --output build/tiny_result.json --paths-output build/tiny.paths
./build/tv4_cli --mode validate --instance data/tiny.mapf --paths build/tiny.paths
python3 scripts/audit_solution.py --instance data/tiny.mapf --solution build/tiny_result.json --oracle --w 1.01
```

Chi tiết batch, thống kê và MovingAI: xem `README.md`.

## Khai báo đóng góp/AI

Code, test và tài liệu được tạo với hỗ trợ AI trong phiên làm việc này. Người nộp cần
đọc, chạy, hiểu thuật toán, chỉnh theo API nhóm, và khai báo AI theo quy định môn học.
Không coi số lượng code hay kết quả test là thay thế cho việc giải thích correctness và giới hạn.
