# MAPF — phần việc TV4

Bộ công cụ C++17/Python cho validator độc lập, joint-state oracle tối ưu SOC,
independent lower bound, gap, thống kê và hook Bypass CBS.
Không cần thư viện Python bên ngoài. Chưa có mã CBS/ECBS của các thành viên khác trong repo.

## 1. Chạy nhanh

Trong terminal:

```bash
cd /home/khoa/dev/multi_agent_path
bash scripts/build.sh
./build/tv4_demo
```

Script build tự chạy CMake, biên dịch và CTest. Trong môi trường hiện tại,
script dùng toolchain đã tải và giải nén tại `/tmp/mapf-toolchain` nếu máy chưa có compiler.
Thư mục `/tmp` có thể bị dọn sau khi khởi động lại. Trên máy Ubuntu của bạn,
cài toolchain một lần nếu chưa có:

```bash
sudo apt update
sudo apt install build-essential cmake python3
```

Sau đó chạy lại `bash scripts/build.sh`. Yêu cầu CMake >= 3.16, compiler C++17,
Python >= 3.9; không cần `pip install`.

Demo dự kiến:

```text
valid=1 SOC=4 makespan=2
oracle=solved optimal_SOC=4
independent_LB=4
bypass=1 conflicts=1 -> 0
```

## 2. Tìm nghiệm tối ưu và kiểm tra lại

```bash
./build/tv4_cli --mode oracle --instance data/tiny.mapf --timeout 10 --output build/tiny_result.json --paths-output build/tiny.paths
./build/tv4_cli --mode validate --instance data/tiny.mapf --paths build/tiny.paths
```

Kết quả `tiny`: `status=solved`, `cost=4`, `certified_lb=4`; validator báo
`validity=true`, `makespan=2`. JSON oracle chứa cả paths để sử dụng với script audit.

Hai ca khó có sẵn:

```bash
./build/tv4_cli --mode oracle --instance data/leave_goal.mapf
./build/tv4_cli --mode oracle --instance data/swap.mapf
```

`leave_goal`: SOC tối ưu 4, agent ở goal phải tạm rời đi rồi quay lại.
`swap`: hai agent đổi chỗ trong hành lang 1×2, kết luận `infeasible`.

Oracle mặc định tối đa 3 agent, 500000 records, timeout 10 giây. Có thể chỉnh
`--max-agents`, `--max-states`, `--timeout`; tăng giới hạn có thể tốn nhiều RAM.
`resource_limit` hoặc `timeout` không có SOC tối ưu; `cost=null`.
`infeasible` chỉ xuất khi duyệt hết không gian hữu hạn, không dùng timeout để kết luận vô nghiệm.
Mã thoát 0 cho các kết quả xử lý bình thường, gồm oracle chứng minh vô nghiệm;
2 cho lời giải invalid, lỗi input, timeout hoặc resource limit. Luôn đọc trường `status`.

## 3. Lower bound và gap

```bash
./build/tv4_cli --mode lb --instance data/tiny.mapf
./build/tv4_cli --mode gap --soc 12 --bound 10 --bound-kind optimum
./build/tv4_cli --mode gap --soc 12 --bound 8 --bound-kind lb
```

Hai ví dụ gap lần lượt trả 0.2 (exact) và 0.5 (upper bound).
Gap tương đối với bound bằng 0 trả `null` vì không thể chia cho 0.
Nguồn optimum/bound do bên gọi cung cấp phải được kiểm chứng; mode gap chỉ tính toán,
không tự xác nhận một số nhập tay là optimum.
Không xuất frontier lower bound từ CBS/ECBS khi chưa có snapshot frontier hợp lệ.

## 4. Audit lời giải CBS/ECBS của nhóm

Ví dụ chạy trọn quy trình bằng lời giải oracle đã tạo ở bước 2:

```bash
python3 scripts/audit_solution.py --instance data/tiny.mapf --solution build/tiny_result.json --oracle --w 1.01 --output build/tiny_audit.json
```

Để kiểm tra solver của nhóm, xuất JSON theo schema:

```json
{"status":"solved","cost":4,"paths":[[[0,0],[0,1],[0,2]],[[1,2],[1,1],[1,0]]]}
```

Thay `--solution` bằng file đó. Audit tự chạy validator, đối chiếu cost báo cáo,
lấy independent LB, và chạy oracle khi có `--oracle` (dành cho instance nhỏ, <=3 agent).
Oracle solved thì kiểm tra `SOC <= w * SOC*`. Nếu oracle timeout/resource limit,
chỉ dùng LB: `SOC <= w * LB` đủ để xác nhận bound; nếu không đạt, kết quả là
`null` (chưa kết luận), không phải vi phạm. Với instance lớn, bỏ `--oracle`.
`reported_cost_matches=false`, path invalid hoặc vi phạm bound có optimum thì script thoát 2.

## 5. Chạy nhiều instance nhỏ và resume

```bash
python3 scripts/run_batch.py --manifest data/tiny_manifest.csv --output build/oracle_batch.csv
```

Manifest có cột `instance_id,instance`; đường dẫn instance tính từ thư mục manifest.
Chạy lại cùng lệnh sẽ bỏ qua các dòng đã ghi. Fingerprint kiểm tra input, binary,
timeout và max-states; thay đổi thì dùng output mới để tránh trộn dữ liệu.
Runner chạy tuần tự, lưu metadata CPU/RAM/Python/concurrency và hash binary bên cạnh CSV.
Đây là runner oracle nhỏ của TV4, không thay runner benchmark CBS/ECBS chính của TV3.

## 6. Phân tích thống kê

```bash
python3 scripts/analyze_results.py data/results_example.csv --a CBS --b ECBS --timeout 60 --output build/statistics_example.json
```

`data/results_example.csv` là **dữ liệu giả để minh họa**, không phải kết quả benchmark.
Dữ liệu thực phải có các cột:

```text
instance_id,solver,status,validity,runtime,cost
```

- Một dòng duy nhất cho mỗi `(instance_id, solver)`; hai solver phải có cùng tập instance.
- `runtime` là giây, hữu hạn và không âm; `cost` là SOC nguyên không âm khi solved+valid.
- Chỉ tính success khi `status=solved`, `validity=true`, runtime < timeout.
- Mỗi cấu hình w/chọn conflict phải dùng tên solver riêng, ví dụ `ECBS_w1.05`.
- Phân tích từng map/mức agent hoặc tập độc lập phù hợp bằng CSV riêng.
- Các prefix nhiều mức agent của cùng scenario có thể phụ thuộc nhau; không gộp
  rồi diễn giải p-value như những mẫu độc lập. Với seed lặp, tổng hợp theo instance trước.

Output có median/IQR, success rate, exact McNemar, Wilcoxon runtime trên jointly-solved,
bootstrap CI 95% của median chênh lệch runtime (seed cố định) và hiệu chỉnh Holm cho
hai phép thử. Wilcoxon dùng phân phối sign-flip exact khi <=100 chênh lệch khác 0,
trên đó dùng xấp xỉ chuẩn có điều chỉnh ties. Đọc các hạn chế trong JSON trước khi kết luận.
Các dòng `validity=true` phải được validator độc lập kiểm tra từ paths trước đó.

## 7. Dữ liệu MovingAI

```bash
python3 scripts/convert_movingai.py --map /path/to/map.map --scen /path/to/map-random-1.scen --agents 3 --output build/from_movingai.mapf
./build/tv4_cli --mode oracle --instance build/from_movingai.mapf --timeout 10
```

Chuyển k dòng đầu scenario thành k agent đồng thời. Hỗ trợ header version 1/1.0,
terrain `. G @ O T`, không hỗ trợ bản đồ swamp/water hoặc scale kích thước.
Scenario dùng tọa độ x/y, công cụ chuyển thành row/col.
Bỏ qua trường khoảng cách có sẵn vì nó có thể dùng mô hình đường chéo;
LB được tính lại bằng BFS 4 hướng.
Tham khảo [định dạng chính thức MovingAI](https://movingai.com/benchmarks/formats.html).

## 8. Quy ước dữ liệu nội bộ

Tọa độ `(row, col)` từ 0; `.` đi được, `@`/`#` chặn. Move 4 hướng hoặc wait.
Mỗi path bắt đầu tại t=0. Path ngắn được kéo dài bằng đứng tại goal.
SOC = tổng thời điểm đến goal lần cuối và ở đó mãi; agent được phép rời goal rồi quay lại.
Wait trước final arrival tính cost, trailing wait tại goal không tính.
Cấm vertex conflict và edge swap; cho phép theo sau và xoay vòng >=3 agent khi không vi phạm hai điều đó.

File `.mapf`: `height width`, các dòng grid, số agent, rồi mỗi agent một dòng
`start_row start_col goal_row goal_col`. File `.paths`: số agent, sau đó mỗi agent một dòng
`path_length row0 col0 row1 col1 ...`. Xem các file trong `data/`.

## 9. Tích hợp và tài liệu

- `include/mapf/validator.hpp`: validate và validate_with_constraints (vertex/edge, cả future goal).
- `include/mapf/joint_state_oracle.hpp`: oracle SOC tối ưu, status/metrics/paths.
- `include/mapf/analysis.hpp`: BFS LB và gap.
- `include/mapf/bypass.hpp`: hook đánh giá Bypass, kèm ví dụ trong demo.
- [Báo cáo hoàn thành](docs/BAO_CAO_TV4.md).
- [Thuật toán, correctness, complexity và Bypass](docs/PHAN_TICH_TV4.md).
- [Dàn ý video](docs/VIDEO_TV4.md).

Bypass hiện là hook đã test, chưa tích hợp cây CBS vì repo chưa có CBS của TV1.
Benchmark MovingAI chính, số liệu CBS/ECBS và báo cáo thực nghiệm cuối cùng cần dữ liệu của nhóm.
JSON có peak memory của process trên Unix/macOS; nền tảng khác có thể trả `null`.
Runtime oracle chỉ đo hàm search, không gồm đọc file, xuất JSON hoặc BFS bổ sung trong CLI.

Mã trong repo được viết trong phiên hỗ trợ AI, chưa chép mã từ repo tham khảo.
Nhóm cần khai báo việc dùng AI và ghi commit nguồn nếu sau này tham khảo/tái sử dụng code.
