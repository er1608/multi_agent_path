# Phân tích phần TV4

## Mô hình và validator

MAPF trên grid hữu hạn 4 hướng, wait, timestep nguyên, vertex conflict và edge swap,
stay-at-target sau final arrival. Gọi t_i là thời điểm agent i tới goal và không rời
nữa; SOC = tổng t_i. Tới goal tạm thời rồi rời đi vẫn phải tính chi phí đến lần quay lại cuối.

Validator kiểm tra instance và từng path trước, rồi pad path ngắn tại goal để kiểm tra
mọi cặp agent theo thời gian. Sau horizon dài nhất, mọi agent ở các goal khác nhau,
nên không thể phát sinh conflict mới. SOC bỏ trailing waits bằng cách lùi qua đoạn
suffix tại goal. Hàm validate_with_constraints kiểm tra thêm vertex/edge constraint
ở thời điểm yêu cầu, kể cả sau khi path kết thúc bằng cách pad tại goal.

Với k agent, tổng độ dài đầu vào L, horizon H: thời gian O(L + H*k² + k²), bộ nhớ phụ O(1)
ngoài input và kết quả. k² đến từ kiểm tra start/goal trùng và mọi cặp path.
Constraint checker bổ sung O(C), với C là số constraint.
Các reason hiện ưu tiên báo lỗi đầu tiên, chưa liệt kê toàn bộ lỗi.

## Oracle tối ưu SOC

State chứa vị trí của tất cả agent và một bit finished cho từng agent.
Agent chưa finished được đi/wait, kể cả khi đang ở goal. Agent ở goal có thể chuyển
finished với chi phí 0 và sẽ đứng đó mãi. Mỗi bước đồng bộ có chi phí bằng số agent
chưa finished trước bước đó; agent finished chỉ wait.

Pseudocode:

```text
OPEN <- (0, initial positions, all unfinished)
while OPEN not empty:
    kiểm tra timeout
    lấy state có cost nhỏ nhất; bỏ entry stale
    nếu tất cả finished: dựng paths, validator kiểm tra, trả optimum
    với mỗi agent unfinished đang ở goal:
        relax state với agent đó finished, cost giữ nguyên
    sinh mọi tổ hợp move/wait; loại vertex/edge-swap conflict
    relax successor với cost + số agent unfinished
    nếu vượt số records: resource_limit
trả infeasible chỉ khi OPEN thật sự rỗng
```

Mỗi lời giải MAPF hợp lệ tương ứng một đường trên graph này bằng cách commit agent
đúng tại final arrival. Tổng trọng số của đường là SOC vì agent i trả đúng một đơn vị
cho mỗi tick trước khi commit, tổng là t_i.
Ngược lại, đường đến all-finished tạo lời giải hợp lệ do các bước đã kiểm tra conflict.
Nếu một agent trì hoãn commit dù đã ở goal vĩnh viễn, có thể commit sớm hơn mà
không thay đổi vị trí hay tăng cost. Vì vậy minimum graph cost đúng bằng SOC tối ưu.
Dijkstra đúng trên graph hữu hạn với trọng số không âm, kể cả cạnh commit bằng 0.
Khi pop all-finished với chi phí nhỏ nhất, giá trị là optimum.

Không dùng BFS joint-state với chi phí một cho mỗi tick: cách đó tối ưu makespan,
không tối ưu tổng thời gian của từng agent.

Với v ô đi được và k agent, số state S <= v^k * 2^k (thực tế ít hơn do collision và
finished chỉ tại goal). Mỗi state có tối đa B=5^k+k cạnh. E <= S*B.
Mã hiện dùng vector state và hash O(k), lưu record bất biến để dựng parent an toàn,
priority queue có thể giữ entry stale. Cận thời gian bảo thủ O(E*(k+log E)),
bộ nhớ O(E*k) cho records/queue trong trường hợp có nhiều improvement; bảng best O(S*k).
max_states giới hạn tổng records (không chỉ unique states), timeout kiểm tra cả khi sinh actions.
Đây là oracle instance nhỏ, không nhằm thay CBS/ECBS trên benchmark lớn.
Không có horizon tùy ý; exhaustive infeasible là kết luận trên graph hữu hạn hoàn chỉnh.

## Lower bound, gap và ECBS

Với mỗi agent, BFS bỏ qua agent khác tìm d_i. Mọi lời giải hợp lệ có t_i >= d_i,
nên LB = tổng d_i <= SOC*. Nếu một goal không reachable kể cả khi bỏ agent khác,
instance MAPF vô nghiệm. Ngược lại, mọi goal individually reachable không bảo đảm joint feasibility.
BFS cho k agent có thời gian O(k*(V+E_grid)), bộ nhớ phụ O(V) và O(k) để lưu distances.

- Có optimum C*: exact relative gap = (C-C*)/C*, ratio = C/C*.
- Chỉ có LB>0: (C-LB)/LB là upper bound của relative gap thực.
- Bound bằng 0: relative gap/ratio không xác định; trả null.
- Chưa có solution valid: không tính gap.
- SOC <= w*C* xác nhận bound trên instance đã kiểm tra.
- SOC <= w*LB cũng đủ xác nhận vì LB <= C*; không đạt điều kiện này thì chưa thể
  kết luận vi phạm khi không biết C*.

Oracle là kiểm chứng hữu hạn bằng thực nghiệm, không thay chứng minh tổng quát ECBS.
Chưa có frontier certificate vì chưa có frontier CBS/ECBS nhất quán trong repo.
Independent LB là nguồn certificate mặc định ngoài optimum oracle.

## Bypass CBS

Ý tưởng: một candidate có cùng chi phí từng agent nhưng ít conflict hơn có thể thay
paths của parent CT node mà chưa cần giữ một nhánh constraint mới, nếu paths vẫn
thỏa toàn bộ constraints của parent. Nó giảm branching trong các trường hợp phù hợp,
nhưng cần cân nhắc chi phí sinh candidate và đếm conflict.

Hook evaluate_bypass kiểm tra:

1. Instance hợp lệ, mỗi path riêng hợp lệ theo start/goal/move/wait.
2. Chi phí của mỗi agent không đổi giữa parent và candidate.
3. Callback bắt buộc xác nhận candidate thỏa mọi constraint của parent.
4. Số conflict giảm nghiêm ngặt, sau khi chuẩn hóa trailing waits.

Không yêu cầu candidate hết conflict: parent/candidate trung gian của CBS có thể còn conflict.
Callback của TV1 cần kiểm tra constraints riêng, không từ chối chỉ vì còn conflict giữa agents.
Dùng trên CBS với các đường low-level tối ưu và node bounds đúng; hook không tự chứng minh
low-level optimality và không tự cập nhật OPEN/FOCAL/CT metadata.

Tích hợp dự kiến tại TV1: sinh candidate từ child replan, gọi hook, nếu accepted thì giữ
constraints của parent, thay paths, tính lại conflicts/policy metadata, xử lý queue entry stale
rồi tiếp tục expand parent. Nếu rejected thì giữ splitting CBS bình thường.
Đừng mang constraint mới của child vào parent mà bỏ sibling; có thể mất completeness.
Không áp dụng hook này cho ECBS nếu chưa có phân tích bound riêng.

Chi phí hook O(L+H*k²) với L/H tổng độ dài/horizon hai bộ paths, ngoài callback constraints.
Để ablation: chạy cùng manifest với bypass off/on, ghi runtime, CT expanded, LL calls,
SOC và overhead. SOC của CBS phải giữ nguyên trên instance có optimum.
Hiện chưa thể chạy ablation CT vì repo chưa có CBS core.

## Thống kê và kiểm soát kết luận

Median/IQR dùng nội suy tuyến tính tại percentile 25/75. McNemar dùng hai số lượng
bất đồng success trên cùng instance, exact binomial hai phía. Wilcoxon dùng runtime
difference A-B trên jointly-solved valid instances, bỏ zero difference và average ranks cho ties.
Exact sign-flip cho <=100 mẫu khác 0; trên đó xấp xỉ chuẩn với variance theo tied ranks.
CI bootstrap percentile 95% cho median paired differences, 2000 lần, seed cố định.
Holm điều chỉnh hai p-value xuất trong cùng báo cáo, chưa điều chỉnh cho mọi map/w/config.

Những điều kiện cần nêu trong report: selection bias trên jointly-solved; giả định
symmetry của Wilcoxon; phụ thuộc giữa các prefix scenario/agent levels và seed repeats;
ảnh hưởng CPU concurrency; số mẫu nhỏ không bảo đảm kết luận mạnh.
Không loại timeout rồi kết luận một solver luôn nhanh hơn; xem success cùng runtime.

Dữ liệu ví dụ trong repo là synthetic, không dùng làm kết quả nghiên cứu.
Nguồn định dạng benchmark: https://movingai.com/benchmarks/formats.html.
Repo đề cung cấp để tham khảo solver: https://github.com/Jiaoyang-Li/EECBS.
Không có mã nguồn sao chép từ repo đó trong phần triển khai hiện tại.
