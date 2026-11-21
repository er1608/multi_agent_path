# Dàn ý video TV4 — khoảng 5 phút 30 giây

Đây là dàn ý để luyện nói; số liệu CBS/ECBS thật cần bổ sung sau khi nhóm chạy benchmark.

- 0:00–0:40: Vai trò TV4: kiểm chứng độc lập, SOC final arrival, khác với makespan.
- 0:40–1:30: Validator: input, move/wait, vertex/edge swap, stay-at-target.
  Demo `./build/tv4_cli --mode validate --instance data/tiny.mapf --paths data/tiny.paths`.
- 1:30–2:45: Joint-state Dijkstra, finished bits, zero-cost commitment, step cost số agent
  chưa finished. Giải thích vì sao BFS mỗi tick không tối ưu SOC. Demo `leave_goal.mapf`.
- 2:45–3:35: Correctness, không gian state tăng theo k; oracle nhỏ dùng đối chiếu CBS/ECBS.
  Nêu 64 ca một agent so BFS và 144 ca hai agent so liệt kê paths độc lập.
- 3:35–4:20: Independent LB, exact gap so optimum khác upper bound khi chỉ có LB.
  Demo audit_solution.py với --oracle --w 1.01; kết quả sample SOC=4, gap=0.
- 4:20–5:00: Median/IQR, paired success McNemar, jointly-solved Wilcoxon; cảnh báo
  selection bias và scenario phụ thuộc. Không trình bày CSV synthetic như benchmark thật.
- 5:00–5:30: Bypass ý tưởng, điều kiện giữ cost/constraints và giảm conflict; hook đã test,
  tích hợp và ablation với CBS của TV1 là bước nhóm cần làm khi có core.

Khi có dữ liệu thật, thay phần thống kê bằng một bảng/biểu đồ của nhóm và nêu rõ map,
số agent, số scenario, timeout, máy chạy. Mỗi thành viên cần hiểu và giải thích được mã mình nộp.
