# 3D Enclosure / Case — ESP32-C6 Outdoor Tracker

Thư mục chứa các tệp thiết kế và mô hình 3D cho vỏ thiết bị Tracker ngoài trời (Outdoor Tracker).

## Cấu Trúc Thư Mục Khuyến Nghị

```text
hardware/case/
├── cad/          # File thiết kế gốc (STEP, Fusion 360, FreeCAD, SolidWorks, ...)
├── stl/          # File mô hình 3D xuất ra sẵn sàng để in (.stl, .3mf)
└── README.md     # Tài liệu và thông số in khuyến nghị
```

## Khuyến Nghị In 3D Cho Thiết Bị Ngoài Trời

* **Vật liệu khuyên dùng:**
  * **PETG** hoặc **ABS/ASA**: Chịu nhiệt độ cao khi để ngoài trời/dưới nắng, chống tia UV tốt hơn nhiều so với PLA (PLA dễ bị biến dạng, giòn khi phơi nắng hoặc trong ô tô/xe máy).
* **Độ dày lớp in (Layer Height):** 0.16mm - 0.20mm
* **Mật độ điền (Infill):** 20% - 40% (Gyroid hoặc Grid)
* **Số lớp viền (Perimeters/Walls):** 3 - 4 lớp để đảm bảo độ cứng vững và chống ẩm/nước tốt hơn.
* **Gia công chống nước (Weatherproofing):**
  * Nên thiết kế rãnh đệm cao su (O-ring hoặc gioăng silicon) giữa nắp trên và nắp dưới.
  * Các cổng hở (USB Type-C, thẻ nhớ Micro SD, nút nguồn) nên có nắp che silicon.
