# 装置A光柱方向问题检查清单

## 问题现象
光柱方向与预期不符，出现"歪"的情况。

## 诊断步骤

### 步骤1：查看诊断日志
1. 运行游戏/编辑器
2. 打开 `Window` → `Developer Tools` → `Output Log`
3. 搜索 `[DeviceA DIAG]` 标签
4. 观察以下信息（每秒输出一次）：
   - **Arrow Location** - 箭头组件的位置
   - **Arrow Forward Vector** - 箭头的前方向量（应该指向光束方向）
   - **Arrow Rotation** - 箭头的旋转（Pitch, Yaw, Roll）
   - **Actor Location/Rotation** - Actor本身的位置和旋转
   - **Beam Start/End** - 光束的起点和终点
   - **Beam Direction** - 计算出的光束方向
   - **BeamComponent Location/Rotation** - Niagara组件的变换

### 步骤2：检查关键问题点

#### 问题1：坐标系统不一致
**症状**：光束起点/终点位置正确，但方向不对

**可能原因**：
- Niagara系统使用的是**本地坐标**，但代码传入的是**世界坐标**
- 或者相反

**检查方法**：
- 查看日志中的 `BeamComponent Location` 和 `Arrow Location`
- 如果它们不同，说明 Niagara 组件有位置偏移
- 检查 Niagara 组件是否应该使用本地坐标

**解决方案**：
- 如果 Niagara 需要本地坐标，需要将世界坐标转换为本地坐标：
```cpp
FVector LocalStart = BeamComponent->GetComponentTransform().InverseTransformPosition(BeamStartLocation);
FVector LocalEnd = BeamComponent->GetComponentTransform().InverseTransformPosition(BeamEndLocation);
```

#### 问题2：Niagara组件旋转不正确
**症状**：光束位置对，但方向被旋转了

**检查方法**：
- 查看日志中的 `BeamComponent Rotation`
- 对比 `Arrow Rotation` 和 `BeamComponent Rotation`
- 如果不同，说明 Niagara 组件有额外的旋转

**解决方案**：
- 在蓝图中检查 Niagara 组件的 `Relative Rotation`
- 应该设置为 (0, 0, 0) 或者与箭头组件对齐

#### 问题3：箭头组件方向设置错误
**症状**：箭头本身指向错误方向

**检查方法**：
- 在编辑器中查看箭头组件的方向（红色箭头）
- 查看日志中的 `Arrow Forward Vector`
- 确认箭头是否指向你期望的方向

**解决方案**：
- 在蓝图中调整箭头组件的 `Relative Rotation`
- 或者调整 Actor 本身的旋转

#### 问题4：Niagara光束系统配置问题
**症状**：代码数据正确，但视觉效果不对

**检查方法**：
- 确认 Niagara 系统中 `User.BeamStart` 和 `User.BeamEnd` 参数已创建
- 检查 Niagara 光束模块是否正确使用了这些参数
- 确认光束模块的坐标空间设置（World Space vs Local Space）

**解决方案**：
- 在 Niagara 编辑器中检查光束模块的坐标空间
- 如果使用 World Space，确保传入世界坐标
- 如果使用 Local Space，需要转换为本地坐标

### 步骤3：可视化调试

#### 方法1：绘制调试线条
代码中已包含 `DrawDebugHelpers.h`，可以添加调试绘制：

```cpp
// 在 UpdateBeam() 末尾添加
DrawDebugLine(World, BeamStartLocation, BeamEndLocation, FColor::Green, false, 0.0f, 0, 5.0f);
DrawDebugSphere(World, BeamStartLocation, 10.0f, 12, FColor::Red, false, 0.0f);
DrawDebugSphere(World, BeamEndLocation, 10.0f, 12, FColor::Blue, false, 0.0f);
```

这样可以直观看到：
- **绿色线条**：代码计算的光束方向
- **红色球**：光束起点
- **蓝色球**：光束终点

对比绿色线条和实际 Niagara 光束，如果不一致，说明是 Niagara 配置问题。

#### 方法2：在编辑器中检查
1. 选中装置A Actor
2. 查看 `ArrowComponent` 的变换
3. 查看 `BeamComponent` 的变换
4. 确认它们的相对位置和旋转

### 步骤4：常见问题快速检查

| 问题 | 检查项 | 解决方法 |
|------|--------|----------|
| 光束起点不对 | 对比 `Arrow Location` 和 `Beam Start` | 检查 Niagara 组件位置偏移 |
| 光束方向不对 | 对比 `Arrow Forward` 和 `Beam Direction` | 检查坐标系统（世界/本地） |
| 光束被旋转 | 对比 `Arrow Rotation` 和 `BeamComponent Rotation` | 重置 Niagara 组件旋转 |
| 光束长度不对 | 检查 `Beam Length` 是否等于 `MaxBeamRange` | 检查射线检测是否命中物体 |

## 下一步操作

根据诊断日志的结果，告诉我：
1. `Arrow Forward Vector` 的值是什么？
2. `Beam Direction` 的值是什么？
3. 它们是否匹配（Dot Product 是否接近 1.0）？
4. `BeamComponent Rotation` 是什么？
5. 在编辑器中，箭头指向哪个方向？

有了这些信息，我可以帮你精确定位问题并提供修复方案。
