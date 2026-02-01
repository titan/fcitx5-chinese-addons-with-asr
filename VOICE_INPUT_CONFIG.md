# Table 输入法语音识别配置说明

## 概述

Table 输入法现已集成火山引擎(豆包语音)的语音识别功能。用户可以通过 GUI 配置界面轻松设置火山引擎 API 参数,实现语音输入。

## 获取火山引擎 API 参数

1. 访问火山引擎控制台: https://console.volcengine.com/ark
2. 创建应用并开通"一句话识别"或"流式语音识别"服务
3. 在应用详情页面获取以下参数:
   - **AppID**: 应用标识
   - **Token**: 访问令牌
   - **Cluster ID**: 业务集群标识(Cluster ID)

## 配置方法

### 方法 1: 通过 fcitx5-configtool GUI 配置

1. 打开 fcitx5 配置工具:
   ```bash
   fcitx5-configtool
   ```

2. 找到"Table"或"码表"输入法配置

3. 在配置界面中找到"语音输入"(Voice Input)相关配置项:

   - **Enable Voice Input (启用语音输入)**: 勾选以启用语音输入功能
   - **Volcengine AppID**: 填写从火山引擎控制台获取的 AppID
   - **Volcengine Token**: 填写从火山引擎控制台获取的 Token
   - **Volcengine Cluster ID**: 填写从火山引擎控制台获取的 Cluster ID

4. 点击"应用"或"确定"保存配置

5. 重启 fcitx5 使配置生效:
   ```bash
   fcitx5-remote -r
   ```

### 方法 2: 手动编辑配置文件

编辑配置文件 `~/.config/fcitx5/conf/table.conf`:

```ini
[Table/VoiceInput]
VoiceInputEnabled=true
VoiceInputAppId=your_appid_here
VoiceInputToken=your_token_here
VoiceInputCluster=your_cluster_id_here

[Table]
# ... 其他配置项 ...
```

替换以下占位符:
- `your_appid_here`: 您的火山引擎 AppID
- `your_token_here`: 您的火山引擎 Token
- `your_cluster_id_here`: 您的火山引擎 Cluster ID

## 使用方法

1. 切换到 Table 输入法

2. 同时按住左右两个 Shift 键开始录音

3. 松开任意一个 Shift 键停止录音并开始识别

4. 识别结果会自动插入到当前输入位置

## 鉴权方式

当前实现使用火山引擎推荐的 **Token 鉴权**方式:

```
Authorization: Bearer; {token}
```

这是最简单和推荐的鉴权方式。

## API 文档参考

- **鉴权方法**: https://www.volcengine.com/docs/6561/107789?lang=zh
- **一句话识别 API**: https://www.volcengine.com/docs/6561/80816?lang=zh

## 技术说明

### 音频格式

- 采样率: 16000 Hz
- 采样位数: 16 bit
- 声道数: 单声道(mono)
- 编码格式: PCM (raw)

### API 端点

```
wss://openspeech.bytedance.com/api/v2/asr
```

### 请求流程

1. 建立 WebSocket 连接,带上鉴权头
2. 发送 full client request (JSON 格式,包含 appid, token, cluster 等参数)
3. 分批发送音频数据 (audio only request)
4. 接收识别结果 (full server response)
5. 关闭连接

## 故障排查

### 语音输入不工作

1. 检查是否启用了语音输入: `VoiceInputEnabled=true`
2. 检查 API 参数是否正确填写
3. 检查火山引擎账户是否开通了语音识别服务
4. 查看日志: `journalctl -u fcitx5 -f`

### 识别失败

1. 确认 AppID, Token, Cluster ID 是否正确
2. 确认火山引擎账户状态是否正常
3. 确认是否有足够的调用配额
4. 检查麦克风权限

### 配置未生效

1. 重启 fcitx5: `fcitx5-remote -r`
2. 检查配置文件路径是否正确: `~/.config/fcitx5/conf/table.conf`
3. 检查配置文件格式是否正确(特别是 section 名称)

## 注意事项

1. Token 是敏感信息,请妥善保管
2. 火山引擎 API 有调用频率和次数限制
3. 语音识别需要网络连接
4. 音频数据会发送到火山引擎服务器进行处理

## 开发者信息

- 项目: fcitx5-chinese-addons-with-asr
- 语音识别服务: 火山引擎(字节跳动)
- API 文档: https://www.volcengine.com/docs/6561/
