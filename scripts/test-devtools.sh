#!/bin/bash

echo "========================================"
echo "CEF DevTools 诊断工具"
echo "========================================"
echo ""

PORT=9222

echo "1. 检查远程调试端口是否开启..."
if command -v curl &> /dev/null; then
    if curl -s "http://localhost:${PORT}/json" > /dev/null 2>&1; then
        echo "   [成功] 远程调试端口 ${PORT} 已开启"
        echo ""
        echo "2. 获取调试目标列表..."
        curl -s "http://localhost:${PORT}/json" | python3 -m json.tool 2>/dev/null || curl -s "http://localhost:${PORT}/json"
        echo ""
        echo "3. 访问方式："
        echo "   a) 在浏览器中打开: http://localhost:${PORT}"
        echo "   b) 或直接按F12打开DevTools窗口"
        echo ""
        echo "[提示] 如果F12窗口是白屏："
        echo "   - 等待5-10秒，单进程模式下DevTools需要时间渲染"
        echo "   - 在浏览器中访问上述URL进行远程调试"
        echo "   - 检查应用日志中是否有CEF消息循环运行的记录"
    else
        echo "   [失败] 远程调试端口 ${PORT} 未开启"
        echo ""
        echo "可能的原因："
        echo "   1. 应用程序未运行"
        echo "   2. CEF未正确初始化"
        echo "   3. 端口被其他程序占用"
        echo ""
        echo "解决方案："
        echo "   1. 确认应用程序正在运行"
        echo "   2. 检查日志文件中的CEF初始化记录"
        echo "   3. 使用 'lsof -i:${PORT}' 或 'netstat -an | grep ${PORT}' 检查端口状态"
    fi
else
    echo "   [警告] curl命令未找到，无法测试"
    echo ""
    echo "请手动在浏览器中访问: http://localhost:${PORT}"
fi

echo ""
echo "========================================"
