<?xml version="1.0" encoding="utf-8"?>
<!DOCTYPE TS>
<TS version="2.1" language="zh_CN">
<context>
    <name>CommandPalette</name>
    <message>
        <source>Search Settings</source>
        <translation>搜索设置</translation>
    </message>
    <message>
        <source>Search settings</source>
        <translation>搜索设置</translation>
    </message>
    <message>
        <source>↑↓ move · Enter run · Esc close · &gt; commands only · @ settings only</source>
        <translation>↑↓ 移动 · Enter 运行 · Esc 关闭 · &gt; 仅命令 · @ 仅设置</translation>
    </message>
    <message>
        <source>Commands</source>
        <translation>命令</translation>
    </message>
    <message>
        <source>Settings</source>
        <translation>设置</translation>
    </message>
    <message>
        <source>Settings in Advanced mode</source>
        <translation>高级模式中的设置</translation>
    </message>
    <message>
        <source>Nothing found</source>
        <translation>找不到任何内容</translation>
    </message>
    <message>
        <source> (on)</source>
        <translation> (开)</translation>
    </message>
    <message>
        <source> (off)</source>
        <translation> (关)</translation>
    </message>
</context>
<context>
    <name>ConnectionManager</name>
    <message>
        <source>A background Throne (PID %1) owns Tun on this computer; stop it or use thronectl.</source>
        <translation>这台电脑上后台 Throne (PID %1) 拥有 Tun；停止它或使用 thronectl。</translation>
    </message>
</context>
<context>
    <name>ConnectionsFilterHeader</name>
    <message>
        <source>Disable Filter</source>
        <translation>禁用筛选</translation>
    </message>
    <message>
        <source>Enable Filter</source>
        <translation>启用筛选</translation>
    </message>
    <message>
        <source>Filter...</source>
        <translation>筛选...</translation>
    </message>
</context>
<context>
    <name>ConnectionsTreeModel</name>
    <message>
        <source>Matched: %1 → %2</source>
        <translation>已匹配: %1 → %2</translation>
    </message>
    <message>
        <source>System</source>
        <translation>系统</translation>
    </message>
    <message>
        <source>Process: %1
Active connections: %2
Total traffic: %3↑ %4↓
Total speed: %5/s↑ %6/s↓</source>
        <translation>进程: %1
活动连接数: %2
流量合计: %3↑ %4↓
速度合计: %5/s↑ %6/s↓</translation>
    </message>
    <message>
        <source>Destination: %1
Connections: %2
Process: %3
Protocol: %4
Outbound: %5
Total traffic: %6↑ %7↓
Total speed: %8/s↑ %9/s↓</source>
        <translation>目标: %1
连接数: %2
进程: %3
协议: %4
出站: %5
流量合计: %6↑ %7↓
速度合计: %8/s↑ %9/s↓</translation>
    </message>
    <message>
        <source>Destination: %1
Process: %2
Protocol: %3
Outbound: %4</source>
        <translation>目标: %1
进程: %2
协议: %3
出站: %4</translation>
    </message>
    <message>
        <source>Process / Destination</source>
        <translation>进程/目标</translation>
    </message>
    <message>
        <source>Source</source>
        <translation>来源</translation>
    </message>
    <message>
        <source>Protocol</source>
        <translation>协议</translation>
    </message>
    <message>
        <source>Outbound</source>
        <translation>出站</translation>
    </message>
    <message>
        <source>Traffic</source>
        <translation>流量</translation>
    </message>
    <message>
        <source>Speed</source>
        <translation>速度</translation>
    </message>
    <message>
        <source>Click To Sort By Process</source>
        <translation>点击表示按进程排序</translation>
    </message>
    <message>
        <source>Click To Sort By Source</source>
        <translation>点击表示按来源排序</translation>
    </message>
    <message>
        <source>Click To Sort By Protocol</source>
        <translation>点击表示按协议排序</translation>
    </message>
    <message>
        <source>Click To Sort By Outbound</source>
        <translation>点击表示按出站排序</translation>
    </message>
    <message>
        <source>Click to sort by traffic; right-click to choose total/down/up</source>
        <translation>点击表示按流量排序；右击选择“合计(Σ)/下载(↓)/上传(↑)&quot;</translation>
    </message>
    <message>
        <source>Click to sort by speed; right-click to choose total/down/up</source>
        <translation>点击表示按速度排序；右击选择“合计(Σ)/下载(↓)/上传(↑)&quot;</translation>
    </message>
</context>
<context>
    <name>CoreSupervisor</name>
    <message>
        <source>Throne could not open the local channel the core connects to (%1), so no profile can start.</source>
        <translation>Throne 无法打开核心连接到的本地通道 (%1)，所以无法启动任何配置档。</translation>
    </message>
    <message>
        <source>Core unavailable</source>
        <translation>核心不可用</translation>
    </message>
    <message>
        <source>The core did not connect within %1 s, restarting it.</source>
        <translation>核心在 %1 秒内没有连接，正在重启它。</translation>
    </message>
    <message>
        <source>The core keeps exiting, so it is no longer restarted. Use Restart Core to try again.</source>
        <translation>核心一直在退出，所以不再重启。使用「重启核心」再试一次。</translation>
    </message>
</context>
<context>
    <name>DiagnosticsTab</name>
    <message>
        <source>Performance profile</source>
        <translation>性能剖析</translation>
    </message>
    <message>
        <source>Profiles the Core using pprof. The profile itself does not contain any information about your configs, but if you include the logs too, there will be information about your configs in them.</source>
        <translation>使用 pprof 分析核心性能。该剖析文件本身不包含任何有关你的配置的信息，但如果还包含日志，则其中将包含相关配置的信息。</translation>
    </message>
    <message>
        <source>Records where goroutines wait on locks and channels.</source>
        <translation>记录 goroutines (Go协程)在锁定和通道上等待（资源释放）的位置。</translation>
    </message>
    <message>
        <source>Lock contention</source>
        <translation>锁竞争</translation>
    </message>
    <message>
        <source>Execution trace (larger file)</source>
        <translation>执行追踪 (大文件)</translation>
    </message>
    <message>
        <source>Logs contain visited domains and server addresses.</source>
        <translation>日志包含访问过的域名和服务器地址。</translation>
    </message>
    <message>
        <source>Include Throne logs</source>
        <translation>包含 Throne 日志</translation>
    </message>
    <message>
        <source>Start</source>
        <translation>启动</translation>
    </message>
    <message>
        <source>Stop</source>
        <translation>停止</translation>
    </message>
    <message>
        <source>Show in folder</source>
        <translation>在文件夹中显示</translation>
    </message>
    <message>
        <source>Open folder</source>
        <translation>打开文件夹</translation>
    </message>
    <message>
        <source>Logs</source>
        <translation>日志</translation>
    </message>
    <message>
        <source>Saves the Throne log of this session as one file, including the core output shown in the log panel.</source>
        <translation>保存这次会话的 Throne 日志为一个文件，包括显示在日志面板中的核心输出。</translation>
    </message>
    <message>
        <source>Save logs…</source>
        <translation>保存日志...</translation>
    </message>
    <message>
        <source>Saved %1 (%2)</source>
        <translation>已保存 %1 (%2)</translation>
    </message>
    <message>
        <source>The core is not running.</source>
        <translation>核心未运行。</translation>
    </message>
    <message>
        <source>Finishing…</source>
        <translation>正在完成...</translation>
    </message>
    <message>
        <source>Recording… %1 left</source>
        <translation>记录中… 剩余 %1</translation>
    </message>
    <message>
        <source>There is no log file to save.</source>
        <translation>没有要保存的日志文件。</translation>
    </message>
    <message>
        <source>Save logs</source>
        <translation>保存日志</translation>
    </message>
    <message>
        <source>Log files (*.log);;All files (*)</source>
        <translation>日志文件 (*.log);;所有文件 (*)</translation>
    </message>
    <message>
        <source>Cannot write to: %1</source>
        <translation>无法写到: %1</translation>
    </message>
</context>
<context>
    <name>DialogAbout</name>
    <message>
        <source>About Throne</source>
        <translation>关于 Throne</translation>
    </message>
    <message>
        <source>Version:</source>
        <translation>版本:</translation>
    </message>
    <message>
        <source>Open-source and cross platform Desktop Proxy management utility developed as a successor to Nekoray.</source>
        <translation>作为 Nekoray 后继者开发的开源且跨平台的桌面代理管理实用工具。</translation>
    </message>
    <message>
        <source>Support Throne</source>
        <translation>支持 Throne</translation>
    </message>
    <message>
        <source>Unknown</source>
        <translation>未知</translation>
    </message>
</context>
<context>
    <name>DialogAutoSelector</name>
    <message>
        <source>Auto Selector Stats</source>
        <translation>自动选择器统计</translation>
    </message>
    <message>
        <source>Only show profiles with problems</source>
        <translation>仅显示有问题的配置档</translation>
    </message>
    <message>
        <source>Use this profile</source>
        <translation>使用这个配置档</translation>
    </message>
    <message>
        <source>Keep the selected profile in use instead of letting the ranking choose. Useful when several profiles measure much the same and you prefer one of them.
It stays a preference, not a lock: if that profile stops working the selector still moves on, and comes back to your choice once it recovers.</source>
        <translation>保持当前选定的配置档(Profile)继续使用中而不是让排名选择。在多个配置档测量结果非常接近而你更偏于使用其中某个的情况下很有用。
它只是偏好选项，而不是锁定：如果该配置档停止工作，则选择器仍会继续切换，并在其恢复后返回到你的选择。</translation>
    </message>
    <message>
        <source>Back to automatic</source>
        <translation>切回自动</translation>
    </message>
    <message>
        <source>Stop preferring a particular profile and let the ranking decide again.</source>
        <translation>停止优选特定配置档，并再次由排名决定。</translation>
    </message>
    <message>
        <source>Check all now</source>
        <translation>立即检查全部</translation>
    </message>
    <message>
        <source>Re-measure every running profile immediately instead of waiting for the next scheduled check.</source>
        <translation>立即重测每个正在运行的配置档，而不是等待下一次的计划检查。</translation>
    </message>
    <message>
        <source>Re-checking every profile...</source>
        <translation>重新检查每个配置档...</translation>
    </message>
    <message>
        <source>Profile</source>
        <translation>配置档</translation>
    </message>
    <message>
        <source>Status</source>
        <translation>状态</translation>
    </message>
    <message>
        <source>Latency</source>
        <translation>延迟</translation>
    </message>
    <message>
        <source>Jitter</source>
        <translation>抖动</translation>
    </message>
    <message>
        <source>Checks</source>
        <translation>检查数</translation>
    </message>
    <message>
        <source>Connects</source>
        <translation>连接数</translation>
    </message>
    <message>
        <source>Last OK</source>
        <translation>上次通过</translation>
    </message>
    <message>
        <source>Notes</source>
        <translation>备注</translation>
    </message>
    <message>
        <source>Could not change the profile: %1</source>
        <translation>无法更改该配置档: %1</translation>
    </message>
    <message>
        <source>Back to automatic — the selector will choose again.</source>
        <translation>切回自动 - 选择器将再次选择。</translation>
    </message>
    <message>
        <source>Now using your chosen profile.</source>
        <translation>现在正在使用你选择的配置档。</translation>
    </message>
    <message>
        <source>No auto selector is running.</source>
        <translation>没有自动选择器正在运行。</translation>
    </message>
    <message>
        <source>Checks are paused because this machine has no network connection. No profile is being blamed for it, and the ranking is frozen until the connection returns.</source>
        <translation>因本机没有网络连接，检查被暂停。不因此归咎于任何配置档，排名结果会被冻结直至连接恢复。</translation>
    </message>
    <message>
        <source>Last check round %1.</source>
        <translation>上次检查轮次 %1。</translation>
    </message>
    <message>
        <source>Next in %1s.</source>
        <translation>下次在 %1s 后。</translation>
    </message>
    <message>
        <source>Using %1 because you chose it.</source>
        <translation>正在使用 %1 (因你选择它)。</translation>
    </message>
    <message>
        <source>You chose %1, but it is not working right now, so the selector picked another. It will go back to yours once it recovers.</source>
        <translation>你选择了 %1，但它目前不正常工作，因此选择器选取了其他（配置档）。一旦它恢复正常，就会切回你的选择。</translation>
    </message>
    <message>
        <source>Last switch %1 (%2).</source>
        <translation>上次切换 %1 (%2)。</translation>
    </message>
    <message>
        <source>Nothing is working right now — if this holds, the selector will rebuild from the next best profiles.</source>
        <translation>目前没有任何配置档正常工作 - 如果这种情况持续，选择器将从下一个最优配置档开始重建。</translation>
    </message>
    <message>
        <source>%1 ms</source>
        <translation>%1 毫秒</translation>
    </message>
    <message>
        <source>Best %1 ms, worst %2 ms</source>
        <translation>最好 %1 毫秒, 最差 %2 毫秒</translation>
    </message>
    <message>
        <source>How much the latency moves around. The selector prefers steady profiles over fast but erratic ones.</source>
        <translation>延迟波动幅度有多大。选择器更偏向稳定的配置档而不是快速但波动大的。</translation>
    </message>
    <message>
        <source>%1 / %2 ok</source>
        <translation>%1 / %2 通过</translation>
    </message>
    <message>
        <source>Real connection attempts made by apps through this profile.</source>
        <translation>通过这个配置档，由应用发出的实际连接尝试次数。</translation>
    </message>
</context>
<context>
    <name>DialogBasicSettings</name>
    <message>
        <source>Basic Settings</source>
        <translation>基本设置</translation>
    </message>
    <message>
        <source>Enable Authorization</source>
        <translation>启用认证</translation>
    </message>
    <message>
        <source>Custom Inbound</source>
        <translation>自定义入站</translation>
    </message>
    <message>
        <source>Edit</source>
        <translation>编辑</translation>
    </message>
    <message>
        <source>Inbound Username</source>
        <translation>入站用户名</translation>
    </message>
    <message>
        <source>Disable Mixed Inbound</source>
        <translation>禁用混合入站</translation>
    </message>
    <message>
        <source>&lt;html&gt;&lt;head/&gt;&lt;body&gt;&lt;p&gt;Selects a random available port on every run&lt;/p&gt;&lt;/body&gt;&lt;/html&gt;</source>
        <translation>&lt;html&gt;&lt;head/&gt;&lt;body&gt;&lt;p&gt;每次运行时选择一个随机可用端口&lt;/p&gt;&lt;/body&gt;&lt;/html&gt;</translation>
    </message>
    <message>
        <source>Random port</source>
        <translation>随机端口</translation>
    </message>
    <message>
        <source>&lt;html&gt;&lt;head/&gt;&lt;body&gt;&lt;p&gt;Socks
                                                                + HTTP Proxy&lt;/p&gt;&lt;/body&gt;&lt;/html&gt;
                                                            </source>
        <translation>&lt;html&gt;&lt;head/&gt;&lt;body&gt;&lt;p&gt;Socks+ HTTP 代理&lt;/p&gt;&lt;/body&gt;&lt;/html&gt;
</translation>
    </message>
    <message>
        <source>Listen Port</source>
        <translation>监听端口</translation>
    </message>
    <message>
        <source>Listen Address</source>
        <translation>监听地址</translation>
    </message>
    <message>
        <source>Inbound Password</source>
        <translation>入站密码</translation>
    </message>
    <message>
        <source>Proxy Format</source>
        <translation>代理格式</translation>
    </message>
    <message>
        <source>Testing</source>
        <translation>测试</translation>
    </message>
    <message>
        <source>Speedtest mode</source>
        <translation>速度测试模式</translation>
    </message>
    <message>
        <source>Download + Upload</source>
        <translation>下载 + 上传</translation>
    </message>
    <message>
        <source>Only Download</source>
        <translation>仅下载</translation>
    </message>
    <message>
        <source>Only Upload</source>
        <translation>仅上传</translation>
    </message>
    <message>
        <source>Simple Download</source>
        <translation>简单下载</translation>
    </message>
    <message>
        <source>Only Country</source>
        <translation>仅国别</translation>
    </message>
    <message>
        <source>&lt;html&gt;&lt;head/&gt;&lt;body&gt;&lt;p&gt;timeout in milliseconds&lt;br/&gt;applies to all tests individually&lt;/p&gt;&lt;/body&gt;&lt;/html&gt;</source>
        <translation>&lt;html&gt;&lt;head/&gt;&lt;body&gt;&lt;p&gt;毫秒为单位的超时值&lt;br/&gt;分别应用于所有测试&lt;/p&gt;&lt;/body&gt;&lt;/html&gt;</translation>
    </message>
    <message>
        <source>Timeout</source>
        <translation>超时</translation>
    </message>
    <message>
        <source>Simple Download URL</source>
        <translation>简单下载 URL</translation>
    </message>
    <message>
        <source>Latency Test URL</source>
        <translation>延迟测试 URL</translation>
    </message>
    <message>
        <source>&lt;html&gt;&lt;head/&gt;&lt;body&gt;&lt;p&gt;Fetched &lt;span style=&quot; font-weight:600;&quot;&gt;without&lt;/span&gt; any proxy, so features that must tell a dead internet connection apart from dead servers — such as the auto selector — can check the connection itself, and notice the moment it comes back.&lt;/p&gt;&lt;p&gt;Only put a URL here that is reachable &lt;span style=&quot; font-weight:600;&quot;&gt;directly&lt;/span&gt; on your network. Leave it empty to rely on the operating system&apos;s network state instead.&lt;/p&gt;&lt;/body&gt;&lt;/html&gt;</source>
        <translation>&lt;html&gt;&lt;head/&gt;&lt;body&gt;&lt;p&gt;&lt;span style=&quot; font-weight:600;&quot;&gt;不用&lt;/span&gt;任何代理获取，因此如自动选择器之类需要区分互联网连接中断和服务器故障的功能，可自行检查网络连接，并在恢复时立即察觉。&lt;/p&gt;&lt;p&gt;仅在这里填写可通过网络&lt;span style=&quot; font-weight:600;&quot;&gt;直接&lt;/span&gt;访问到的 URL。留空则改为依赖系统的网络状态。&lt;/p&gt;&lt;/body&gt;&lt;/html&gt;</translation>
    </message>
    <message>
        <source>Direct Test URL</source>
        <translation>直连测试 URL</translation>
    </message>
    <message>
        <source>optional — use the OS network state</source>
        <translation>可选 - 使用操作系统的网络状态</translation>
    </message>
    <message>
        <source>&lt;html&gt;&lt;head/&gt;&lt;body&gt;&lt;p&gt;After connecting, Throne asks an IP lookup service — through the proxy — which IP and country the connection exits from.&lt;/p&gt;&lt;p&gt;Turn this off to never contact such a service automatically.&lt;/p&gt;&lt;/body&gt;&lt;/html&gt;</source>
        <translation>&lt;html&gt;&lt;head/&gt;&lt;body&gt;&lt;p&gt;连接后，Throne 会通过代理向 IP 查询服务询问该连接从哪个 IP 和国家出口。&lt;/p&gt;&lt;p&gt;关闭这个功能可阻止自动联系这样的服务。&lt;/p&gt;&lt;/body&gt;&lt;/html&gt;</translation>
    </message>
    <message>
        <source>Look up exit country</source>
        <translation>查询出口国家</translation>
    </message>
    <message>
        <source>Lookup service</source>
        <translation>查询服务</translation>
    </message>
    <message>
        <source>Concurrency</source>
        <translation>并发数</translation>
    </message>
    <message>
        <source>&lt;html&gt;&lt;head/&gt;&lt;body&gt;&lt;p&gt;Timeout for URLtest in ms&lt;br/&gt;Note that muxed connections take a much longer time for their initial request, and setting this value too low will cause the test to falsely report that the config is not working&lt;/p&gt;&lt;/body&gt;&lt;/html&gt;</source>
        <translation>&lt;html&gt;&lt;head/&gt;&lt;body&gt;&lt;p&gt;以毫秒为单位的 URL 测试超时&lt;br/&gt;注意，复用连接在其初始请求时耗时更长，且这个值设置过低将会导致测试误报该配置无效&lt;/p&gt;&lt;/body&gt;&lt;/html&gt;</translation>
    </message>
    <message>
        <source>Register throne:// links at startup</source>
        <translation>启动时注册 throne:// 链接</translation>
    </message>
    <message>
        <source>&lt;html&gt;&lt;head/&gt;&lt;body&gt;&lt;p&gt;Registers the handler now, without waiting for the next start.&lt;/p&gt;&lt;/body&gt;&lt;/html&gt;</source>
        <translation>&lt;html&gt;&lt;head/&gt;&lt;body&gt;&lt;p&gt;立即注册该处理工具，无需等待下次启动。&lt;/p&gt;&lt;/body&gt;&lt;/html&gt;</translation>
    </message>
    <message>
        <source>Install</source>
        <translation>安装</translation>
    </message>
    <message>
        <source>Links and Files</source>
        <translation>链接和文件</translation>
    </message>
    <message>
        <source>&lt;html&gt;&lt;head/&gt;&lt;body&gt;&lt;p&gt;Registers Throne as the handler for throne:// links, re-applying the registration whenever the install is moved.&lt;/p&gt;&lt;p&gt;Turning this off does not undo an existing registration: use Uninstall for that.&lt;/p&gt;&lt;/body&gt;&lt;/html&gt;</source>
        <translation>&lt;html&gt;&lt;head/&gt;&lt;body&gt;&lt;p&gt;注册 Throne 为 throne:// 链接的处理程序，在软件安装移动位置时重新应用该注册。&lt;/p&gt;&lt;p&gt;关闭这个选项不会撤销已有的注册：如需撤销请使用“卸载”。&lt;/p&gt;&lt;/body&gt;&lt;/html&gt;</translation>
    </message>
    <message>
        <source>&lt;html&gt;&lt;head/&gt;&lt;body&gt;&lt;p&gt;Removes the throne:// handler Throne wrote. Entries owned by other applications are left alone.&lt;/p&gt;&lt;/body&gt;&lt;/html&gt;</source>
        <translation>&lt;html&gt;&lt;head/&gt;&lt;body&gt;&lt;p&gt;移除 Throne 写入的 throne:// 处理工具。属于其它应用程序的条目不受影响。&lt;/p&gt;&lt;/body&gt;&lt;/html&gt;</translation>
    </message>
    <message>
        <source>Uninstall</source>
        <translation>卸载</translation>
    </message>
    <message>
        <source>&lt;html&gt;&lt;head/&gt;&lt;body&gt;&lt;p&gt;Adds Throne to the &quot;Open with&quot; list for config files such as JSON and YAML, re-applying the registration whenever the install is moved.&lt;/p&gt;&lt;p&gt;Turning this off does not undo an existing registration: use Uninstall for that.&lt;/p&gt;&lt;/body&gt;&lt;/html&gt;</source>
        <translation>&lt;html&gt;&lt;head/&gt;&lt;body&gt;&lt;p&gt;添加 Throne 到（如 JSON 和 YAML 等）配置文件的“打开方式”列表，当该安装移动时重新应用该注册。&lt;/p&gt;&lt;p&gt;关闭这个选项不会撤销已有注册：如需撤销则使用“卸载”功能。&lt;/p&gt;&lt;/body&gt;&lt;/html&gt;</translation>
    </message>
    <message>
        <source>Register config files at startup</source>
        <translation>启动时注册配置文件</translation>
    </message>
    <message>
        <source>&lt;html&gt;&lt;head/&gt;&lt;body&gt;&lt;p&gt;Registers the associations now, without waiting for the next start.&lt;/p&gt;&lt;/body&gt;&lt;/html&gt;</source>
        <translation>&lt;html&gt;&lt;head/&gt;&lt;body&gt;&lt;p&gt;立即注册该关联而无需等待下次启动。&lt;/p&gt;&lt;/body&gt;&lt;/html&gt;</translation>
    </message>
    <message>
        <source>&lt;html&gt;&lt;head/&gt;&lt;body&gt;&lt;p&gt;Removes every config file association Throne wrote, including those added by older versions. Associations owned by other applications are left alone.&lt;/p&gt;&lt;/body&gt;&lt;/html&gt;</source>
        <translation>&lt;html&gt;&lt;head/&gt;&lt;body&gt;&lt;p&gt;移除由 Throne 写入的每个配置文件关联，包括由较旧版本添加的那些。属于其他应用程序的条目会保留不变。&lt;/p&gt;&lt;/body&gt;&lt;/html&gt;</translation>
    </message>
    <message>
        <source>Max log lines</source>
        <translation>最多日志行数</translation>
    </message>
    <message>
        <source>Sing-box Log level</source>
        <translation>Sing-box 日志级别</translation>
    </message>
    <message>
        <source>Xray Log level</source>
        <translation>Xray 日志级别</translation>
    </message>
    <message>
        <source>Log Filtering</source>
        <translation>日志筛选</translation>
    </message>
    <message>
        <source>Include Keywords</source>
        <translation>包含关键字</translation>
    </message>
    <message>
        <source>Include Regex</source>
        <translation>包含正则表达式</translation>
    </message>
    <message>
        <source>Enable Include Rules</source>
        <translation>启用「包含」规则</translation>
    </message>
    <message>
        <source>Enable Exclude Rules</source>
        <translation>启用「排除」规则</translation>
    </message>
    <message>
        <source>Exclude Keywords</source>
        <translation>排除关键字</translation>
    </message>
    <message>
        <source>Exclude Regex</source>
        <translation>排除正则表达式</translation>
    </message>
    <message>
        <source>Font</source>
        <translation>字体</translation>
    </message>
    <message>
        <source>Font Size</source>
        <translation>字体大小</translation>
    </message>
    <message>
        <source>Log Browser Font</source>
        <translation>日志浏览器字体</translation>
    </message>
    <message>
        <source>Only monospaced fonts are listed. An unavailable family falls back to a built-in monospace list.</source>
        <translation>仅列出等宽字体。不可用的字体系列会回退到内置的等宽字体列表。</translation>
    </message>
    <message>
        <source>Log Browser Font Size</source>
        <translation>日志浏览器字体大小</translation>
    </message>
    <message>
        <source>Enable</source>
        <translation>启用</translation>
    </message>
    <message>
        <source>&lt;html&gt;&lt;head/&gt;&lt;body&gt;&lt;p&gt;Fully disables the traffic aggregation feature (per-config and per-app usage history kept in a separate database, shown in the Traffic Stats dashboard). Does not affect the per-profile traffic counters in the main window.&lt;/p&gt;&lt;/body&gt;&lt;/html&gt;</source>
        <translation>&lt;html&gt;&lt;head/&gt;&lt;body&gt;&lt;p&gt;完全禁用流量聚合功能(每个配置和每个应用的使用历史会保存在一个单独的数据库中，显示在“流量统计”仪表板中)。 不影响主窗口中每个配置档的流量计数器。&lt;/p&gt;&lt;/body&gt;&lt;/html&gt;</translation>
    </message>
    <message>
        <source>Disable Traffic Aggregation</source>
        <translation>禁用流量聚合</translation>
    </message>
    <message>
        <source>&lt;html&gt;&lt;head/&gt;&lt;body&gt;&lt;p&gt;Show each config&apos;s security next to its type in the proxy list, e.g. &quot;Trojan (WebSocket+TLS)&quot;. Insecure configs are marked with a warning sign.&lt;/p&gt;&lt;/body&gt;&lt;/html&gt;</source>
        <translation>&lt;html&gt;&lt;head/&gt;&lt;body&gt;&lt;p&gt;在代理列表中在每个配置的类型旁边显示其安全机制信息，例如 &quot;Trojan (WebSocket+TLS)&quot;。不安全的配置会用一个警告符号标记。&lt;/p&gt;&lt;/body&gt;&lt;/html&gt;</translation>
    </message>
    <message>
        <source>Show Config Security</source>
        <translation>显示配置安全信息</translation>
    </message>
    <message>
        <source>Hide dashboard at startup</source>
        <translation>启动时隐藏仪表盘</translation>
    </message>
    <message>
        <source>Prevents the app from minimizing to the tray when the window is closed. The app will exit instead</source>
        <translation>防止应用在窗口关闭时最小化到托盘。而是直接退出</translation>
    </message>
    <message>
        <source>Disable tray</source>
        <translation>禁用托盘</translation>
    </message>
    <message>
        <source>Skip confirmation When Deleting Profiles</source>
        <translation>删除配置档时跳过确认</translation>
    </message>
    <message>
        <source>Theme</source>
        <translation>主题</translation>
    </message>
    <message>
        <source>Select</source>
        <translation>选择</translation>
    </message>
    <message>
        <source>Enable Custom Icons</source>
        <translation>启用自定义图标</translation>
    </message>
    <message>
        <source>Follow the connection status icon in the dock/taskbar. When off, it always shows the Throne icon</source>
        <translation>Dock/任务栏中图标跟随连接状态变化。关闭时始终显示 Throne 图标</translation>
    </message>
    <message>
        <source>Follow Status Icon in Taskbar</source>
        <translation>任务栏图标跟随状态</translation>
    </message>
    <message>
        <source>Language</source>
        <translation>语言</translation>
    </message>
    <message>
        <source>Simple hides the advanced options. They keep their values and still apply.</source>
        <translation>简单模式隐藏高级选项。它们的值会保留并仍然生效。</translation>
    </message>
    <message>
        <source>Interface</source>
        <translation>接口</translation>
    </message>
    <message>
        <source>Simple</source>
        <translation>简单</translation>
    </message>
    <message>
        <source>Advanced</source>
        <translation>高级</translation>
    </message>
    <message>
        <source>hwid=custom_value,os=custom_os,osVersion=custom_version,model=custom_model</source>
        <translation>hwid=自定义值,os=自定义操作系统,osVersion=自定义版本,model=自定义型号</translation>
    </message>
    <message>
        <source>Clear servers before updating subscription</source>
        <translation>更新订阅前清除服务器</translation>
    </message>
    <message>
        <source>&lt;html&gt;&lt;head/&gt;&lt;body&gt;&lt;p&gt;When enabled, a window listing added / updated / deleted profiles is shown after a manual subscription update. The same information is always written to the log.&lt;/p&gt;&lt;/body&gt;&lt;/html&gt;</source>
        <translation>&lt;html&gt;&lt;head/&gt;&lt;body&gt;&lt;p&gt;启用时，手动更新订阅后显示一个窗口，列出增加/更新/删除的配置档。 同样的信息始终写到日志。&lt;/p&gt;&lt;/body&gt;&lt;/html&gt;</translation>
    </message>
    <message>
        <source>Show the changes window after a manual subscription update</source>
        <translation>订阅手动更新后显示变化窗口</translation>
    </message>
    <message>
        <source>Interval (minute, invalid if less than 30)</source>
        <translation>时间间隔 (分钟，少于 30 则无效)</translation>
    </message>
    <message>
        <source>Subscriptions whose server sends an update interval (profile-update-interval) follow it; the interval above applies when a server sends none. Groups can override this in their Advanced settings.</source>
        <translation>如果订阅的服务器发送了更新间隔 (profile-update-interval)，则按服务器设置；上面的间隔是服务器没有发送内容时适用的。分组可以在其高级设置中改写这个设置。</translation>
    </message>
    <message>
        <source>Use the update interval sent by the server</source>
        <translation>使用由服务器发送的更新间隔</translation>
    </message>
    <message>
        <source>&lt;html&gt;&lt;head/&gt;&lt;body&gt;&lt;p&gt;HWID=%1&lt;/p&gt;&lt;p&gt;OS=%2&lt;/p&gt;&lt;p&gt;OS Version=%3&lt;/p&gt;&lt;p&gt;Model=%4&lt;/p&gt;&lt;/body&gt;&lt;/html&gt;</source>
        <translation>&lt;html&gt;&lt;head/&gt;&lt;body&gt;&lt;p&gt;HWID=%1&lt;/p&gt;&lt;p&gt;OS=%2&lt;/p&gt;&lt;p&gt;OS Version=%3&lt;/p&gt;&lt;p&gt;Model=%4&lt;/p&gt;&lt;/body&gt;&lt;/html&gt;</translation>
    </message>
    <message>
        <source>Enable sending HWID, device model, and OS version when updating subscription</source>
        <translation>启用在更新订阅时发送硬件ID、设备型号以及 OS 版本</translation>
    </message>
    <message>
        <source>&lt;html&gt;&lt;head/&gt;&lt;body&gt;&lt;p&gt;Format: hwid=value,os=value,osVersion=value,model=value&lt;/p&gt;&lt;p&gt;Leave empty to use default values. Only specify the parameters you want to override.&lt;/p&gt;&lt;/body&gt;&lt;/html&gt;</source>
        <translation>&lt;html&gt;&lt;head/&gt;&lt;body&gt;&lt;p&gt;格式： hwid=值,os=值,osVersion=值,model=值&lt;/p&gt;&lt;p&gt;留空表示使用默认置。 仅指定您想要改写的参数。&lt;/p&gt;&lt;/body&gt;&lt;/html&gt;</translation>
    </message>
    <message>
        <source>Custom System Parameters (optional)</source>
        <translation>自定义系统参数(可选)</translation>
    </message>
    <message>
        <source>&lt;html&gt;&lt;head/&gt;&lt;body&gt;&lt;p&gt;When enabled, If the currently active profile needs to be removed, it will be stopped and removed, otherwise it will not be removed at all&lt;/p&gt;&lt;/body&gt;&lt;/html&gt;</source>
        <translation>&lt;html&gt;&lt;head/&gt;&lt;body&gt;&lt;p&gt;启用时，如果需要移除当前活动的配置档，则它会被停止并移除，不启用的话则完全不会被移除&lt;/p&gt;&lt;/body&gt;&lt;/html&gt;</translation>
    </message>
    <message>
        <source>Allow stopping the active profile</source>
        <translation>允许停止活动配置档</translation>
    </message>
    <message>
        <source>User Agent</source>
        <translation>User Agent
(用户代理)标识</translation>
    </message>
    <message>
        <source>TLS versions offered when fetching subscriptions. Some servers answer the default handshake with a CAPTCHA or a block page; TLS 1.2 often gets through.</source>
        <translation>获取这个订阅时提供的 TLS 版本。一些服务器会在应答默认握手时返回 CAPTCHA 或拦阻页面；TLS 1.2 通常可以通过。</translation>
    </message>
    <message>
        <source>TLS Version</source>
        <translation>TLS 版本</translation>
    </message>
    <message>
        <source>Auto</source>
        <translation>自动</translation>
    </message>
    <message>
        <source>TLS 1.2</source>
        <translation>TLS 1.2</translation>
    </message>
    <message>
        <source>TLS 1.3</source>
        <translation>TLS 1.3</translation>
    </message>
    <message>
        <source>Auto uses HTTP/2 when the server offers it.</source>
        <translation>当服务器提供时，自动使用 HTTP/2。</translation>
    </message>
    <message>
        <source>HTTP Version</source>
        <translation>HTTP 版本</translation>
    </message>
    <message>
        <source>HTTP/1.1</source>
        <translation>HTTP/1.1</translation>
    </message>
    <message>
        <source>Core</source>
        <translation>核心</translation>
    </message>
    <message>
        <source>Clash API</source>
        <translation>Clash API</translation>
    </message>
    <message>
        <source>Xray Core</source>
        <translation>Xray 核心</translation>
    </message>
    <message>
        <source>&lt;html&gt;&lt;head/&gt;&lt;body&gt;&lt;p&gt;When to choose Xray when importing vless profiles&lt;/p&gt;&lt;/body&gt;&lt;/html&gt;</source>
        <translation>&lt;html&gt;&lt;head/&gt;&lt;body&gt;&lt;p&gt;导入 vless 配置档时何时选择 Xray&lt;/p&gt;&lt;/body&gt;&lt;/html&gt;</translation>
    </message>
    <message>
        <source>Xray VLESS Preference</source>
        <translation>Xray VLESS 首选项</translation>
    </message>
    <message>
        <source>Allow updating to beta versions</source>
        <translation>允许更新到 beta 版</translation>
    </message>
    <message>
        <source>&lt;html&gt;&lt;head/&gt;&lt;body&gt;&lt;p&gt;Use proxy for subscription request, update request etc&lt;/p&gt;&lt;/body&gt;&lt;/html&gt;</source>
        <translation>&lt;html&gt;&lt;head/&gt;&lt;body&gt;&lt;p&gt;使用代理处理订阅请求、更新请求等&lt;/p&gt;&lt;/body&gt;&lt;/html&gt;</translation>
    </message>
    <message>
        <source>&lt;html&gt;&lt;head/&gt;&lt;body&gt;&lt;p&gt;Might Improve Ping and Performance&lt;/p&gt;&lt;/body&gt;&lt;/html&gt;</source>
        <translation>&lt;html&gt;&lt;head/&gt;&lt;body&gt;&lt;p&gt;可能改善延迟和性能&lt;/p&gt;&lt;/body&gt;&lt;/html&gt;</translation>
    </message>
    <message>
        <source>Disable Traffic Stats</source>
        <translation>禁用流量统计</translation>
    </message>
    <message>
        <source>&lt;html&gt;&lt;head/&gt;&lt;body&gt;&lt;p&gt;A DNS server for local use will be deployed on this port&lt;/p&gt;&lt;/body&gt;&lt;/html&gt;</source>
        <translation>&lt;html&gt;&lt;head/&gt;&lt;body&gt;&lt;p&gt;供本地使用的 DNS 服务器将部署在这个端口上&lt;/p&gt;&lt;/body&gt;&lt;/html&gt;</translation>
    </message>
    <message>
        <source>Lets other devices on your network use this DNS server, for example a router that sends its DNS through this computer. Anyone who can reach this port can then resolve names through your DNS settings.</source>
        <translation>让你的网络上的其他设备使用这个 DNS 服务器，例如，通过这台计算机发送其 DNS 请求的路由器。可访问这个端口的任何设备都可以通过你的 DNS 设置解析名称。</translation>
    </message>
    <message>
        <source>Allow other devices to connect</source>
        <translation>允许其他设备连接</translation>
    </message>
    <message>
        <source>&lt;html&gt;&lt;head/&gt;&lt;body&gt;&lt;p&gt;Settings for the sing-box&apos;s built-in NTP client&lt;/p&gt;&lt;/body&gt;&lt;/html&gt;</source>
        <translation>&lt;html&gt;&lt;head/&gt;&lt;body&gt;&lt;p&gt;singbox 的内置 NTP 客户端的设置&lt;/p&gt;&lt;/body&gt;&lt;/html&gt;</translation>
    </message>
    <message>
        <source>NTP Settings</source>
        <translation>NTP 设置</translation>
    </message>
    <message>
        <source>server</source>
        <translation>服务器</translation>
    </message>
    <message>
        <source>Port</source>
        <translation>端口</translation>
    </message>
    <message>
        <source>sync interval</source>
        <translation>同步间隔</translation>
    </message>
    <message>
        <source>&lt;html&gt;&lt;head/&gt;&lt;body&gt;&lt;p&gt;In minutes&lt;/p&gt;&lt;/body&gt;&lt;/html&gt;</source>
        <translation>&lt;html&gt;&lt;head/&gt;&lt;body&gt;&lt;p&gt;分钟为单位&lt;/p&gt;&lt;/body&gt;&lt;/html&gt;</translation>
    </message>
    <message>
        <source>outbound</source>
        <translation>出站</translation>
    </message>
    <message>
        <source>&lt;html&gt;&lt;head/&gt;&lt;body&gt;&lt;p&gt;Which outbound the NTP query is sent through&lt;/p&gt;&lt;/body&gt;&lt;/html&gt;</source>
        <translation>&lt;html&gt;&lt;head/&gt;&lt;body&gt;&lt;p&gt;发送 NTP 查询所通过的出站方式或通道&lt;/p&gt;&lt;/body&gt;&lt;/html&gt;</translation>
    </message>
    <message>
        <source>&lt;html&gt;&lt;head/&gt;&lt;body&gt;&lt;p&gt;URL for geoip.dat, downloaded when a full Xray config&apos;s routing needs geoip: rules.&lt;/p&gt;&lt;/body&gt;&lt;/html&gt;</source>
        <translation>&lt;html&gt;&lt;head/&gt;&lt;body&gt;&lt;p&gt;geoip.dat 的 URL，当完整的 Xray 配置的路由需要 geoip: 规则时（将从该地址）下载。&lt;/p&gt;&lt;/body&gt;&lt;/html&gt;</translation>
    </message>
    <message>
        <source>GeoIP Asset URL</source>
        <translation>GeoIP 资源 URL</translation>
    </message>
    <message>
        <source>Download geoip.dat now from the URL on the left</source>
        <translation>立即从左边的 URL 下载 geoip.dat</translation>
    </message>
    <message>
        <source>Download</source>
        <translation>下载</translation>
    </message>
    <message>
        <source>&lt;html&gt;&lt;head/&gt;&lt;body&gt;&lt;p&gt;URL for geosite.dat, downloaded when a full Xray config&apos;s routing needs geosite: rules.&lt;/p&gt;&lt;/body&gt;&lt;/html&gt;</source>
        <translation>&lt;html&gt;&lt;head/&gt;&lt;body&gt;&lt;p&gt;geosite.dat 的 URL，当完整的 Xray 配置的路由需要 geosite: 规则时下载。&lt;/p&gt;&lt;/body&gt;&lt;/html&gt;</translation>
    </message>
    <message>
        <source>GeoSite Asset URL</source>
        <translation>GeoSite 资源的 URL</translation>
    </message>
    <message>
        <source>Download geosite.dat now from the URL on the left</source>
        <translation>立即从左边的 URL 下载 geosite.dat</translation>
    </message>
    <message>
        <source>Create Backup</source>
        <translation>创建备份</translation>
    </message>
    <message>
        <source>Create a portable backup file. Choose which parts to include:</source>
        <translation>创建便携式备份文件。选择要包含哪些部分:</translation>
    </message>
    <message>
        <source>Profiles (groups and proxies)</source>
        <translation>配置档(分组和代理)</translation>
    </message>
    <message>
        <source>Routing profiles</source>
        <translation>路由配置档</translation>
    </message>
    <message>
        <source>Settings</source>
        <translation>设置</translation>
    </message>
    <message>
        <source>OTP profiles</source>
        <translation>OTP 配置档</translation>
    </message>
    <message>
        <source>IP lists and scans</source>
        <translation>IP 列表和扫描</translation>
    </message>
    <message>
        <source>Custom icons</source>
        <translation>自定义图标</translation>
    </message>
    <message>
        <source>Create Backup...</source>
        <translation>创建备份...</translation>
    </message>
    <message>
        <source>Restore Backup</source>
        <translation>恢复备份</translation>
    </message>
    <message>
        <source>Restore from a backup file. You can choose which parts to restore; each selected part replaces the current data. The application restarts after restore.</source>
        <translation>从备份文件中恢复。你可以选择要恢复的部分；每个选中部分都会替换当前数据。恢复后应用程序重启。</translation>
    </message>
    <message>
        <source>Restore from Backup...</source>
        <translation>从备份恢复...</translation>
    </message>
    <message>
        <source>WebDAV</source>
        <translation>WebDAV</translation>
    </message>
    <message>
        <source>Keep backups on a WebDAV server such as Nextcloud, a NAS or Yandex Disk. Uploads include the parts checked under Create Backup.</source>
        <translation>在 WebDAV 服务器(譬如 Nextcloud)、NAS 或 Yandex Disk 上保留备份。上传内容包含在「创建备份」下勾选的各个部分。</translation>
    </message>
    <message>
        <source>WebDAV Settings...</source>
        <translation>WebDAV 设置...</translation>
    </message>
    <message>
        <source>Upload to WebDAV</source>
        <translation>上传到 WebDAV</translation>
    </message>
    <message>
        <source>Restore from WebDAV...</source>
        <translation>恢复自 WebDAV...</translation>
    </message>
    <message>
        <source>Security</source>
        <translation>安全</translation>
    </message>
    <message>
        <source>While Throne is running, block all traffic that does not go through Throne, including when no profile is running.
The local network stays reachable unless Private Range Bypass is turned off in Tun Settings.</source>
        <translation>当 Throne 正在运行时，阻止所有没有通过 Throne 的流量，包括没有运行任何配置当的时候。
除非在 Tun 设置中关闭了“私有范围绕过”，否则本地网络仍可访问。</translation>
    </message>
    <message>
        <source>Kill Switch</source>
        <translation>急停开关</translation>
    </message>
    <message>
        <source>Disable Privilege request</source>
        <translation>禁止权限请求</translation>
    </message>
    <message>
        <source>Use Mozilla Certificate Store</source>
        <translation>使用 Mozilla 证书存储库</translation>
    </message>
    <message>
        <source>Xray verifies servers only against its built-in root certificates. Turn this on to also trust the certificates installed in the system, such as a company or self-signed root. A profile or Xray config that sets useSystemCA itself keeps its own value.</source>
        <translation>Xray 只会针对其内置的根证书验证服务器。开启这个选项还会信任安装在系统中的证书，譬如公司或自签的根证书。设置 useSystemCA 的配置档或 Xray 配置保留自有值。</translation>
    </message>
    <message>
        <source>Trust System Certificates in Xray</source>
        <translation>在 Xray 中信任系统证书</translation>
    </message>
    <message>
        <source>Skip TLS certificate authentication by default (allowInsecure)</source>
        <translation>默认跳过 TLS 证书验证 (allowInsecure)</translation>
    </message>
    <message>
        <source>&lt;html&gt;&lt;head/&gt;&lt;body&gt;&lt;p&gt;Do not attempt to start as Admin unless explicitly requested&lt;/p&gt;&lt;/body&gt;&lt;/html&gt;</source>
        <translation>&lt;html&gt;&lt;head/&gt;&lt;body&gt;&lt;p&gt;除非明确要求，否则不要尝试以管理员身份启动&lt;/p&gt;&lt;/body&gt;&lt;/html&gt;</translation>
    </message>
    <message>
        <source>Always Start as Standard User</source>
        <translation>始终以标准用户身份启动</translation>
    </message>
    <message>
        <source>Could not register the handler for throne:// links.</source>
        <translation>无法为 throne:// 链接注册处理工具。</translation>
    </message>
    <message>
        <source>Could not register the config file associations.</source>
        <translation>无法注册配置文件关联。</translation>
    </message>
    <message>
        <source>Select png icons</source>
        <translation>选择 PNG 图标</translation>
    </message>
    <message>
        <source>Image Files (*.png)</source>
        <translation>图像文件 (*.png)</translation>
    </message>
    <message>
        <source>Diagnostics</source>
        <translation>诊断</translation>
    </message>
    <message>
        <source>Not available for this installation</source>
        <translation>不可用于这个安装</translation>
    </message>
    <message>
        <source>Installed</source>
        <translation>已安装</translation>
    </message>
    <message>
        <source>Not installed</source>
        <translation>未安装</translation>
    </message>
    <message>
        <source>Download geo asset</source>
        <translation>下载 geo 资源</translation>
    </message>
    <message>
        <source>Please enter a URL for %1 first.</source>
        <translation>请首先输入一个 %1 的 URL。</translation>
    </message>
    <message>
        <source>Downloading Xray geo asset: %1</source>
        <translation>正在下载 Xray geo 资源: %1</translation>
    </message>
    <message>
        <source>Select at least one part to include in the backup.</source>
        <translation>至少选择一个部分以包含在备份中。</translation>
    </message>
    <message>
        <source>Throne Backup (*.thrbackup)</source>
        <translation>Throne 备份 (*.thrbackup)</translation>
    </message>
    <message>
        <source>Backup Failed</source>
        <translation>备份失败</translation>
    </message>
    <message>
        <source>Failed to create database snapshot: %1</source>
        <translation>创建数据库快照失败: %1</translation>
    </message>
    <message>
        <source>Failed to read database snapshot.</source>
        <translation>读取数据库快照失败。</translation>
    </message>
    <message>
        <source>Cannot write to: %1</source>
        <translation>无法写到: %1</translation>
    </message>
    <message>
        <source>Profiles</source>
        <translation>配置档</translation>
    </message>
    <message>
        <source>Backup Created</source>
        <translation>备份已创建</translation>
    </message>
    <message>
        <source>Backup created successfully:
%1
Included: %2</source>
        <translation>备份成功创建:
%1
包括: %2</translation>
    </message>
    <message>
        <source>Restore Failed</source>
        <translation>恢复失败</translation>
    </message>
    <message>
        <source>Cannot open backup file: %1</source>
        <translation>无法打开备份文件: %1</translation>
    </message>
    <message>
        <source>Enter a whole number of milliseconds, for example %1 (without a unit), or 0 for the default.</source>
        <translation>输入一个整数毫秒，例如 %1 (不带单位)，或者 0 表示使用默认值。</translation>
    </message>
    <message>
        <source>Enter a whole number of milliseconds, for example %1 (without a unit).</source>
        <translation>输入一个整数毫秒，例如 %1 (不带单位)。</translation>
    </message>
    <message>
        <source>Must be shorter than 1000 characters, on a single line.</source>
        <translation>必须少于 1000 个字符，且在一行上。</translation>
    </message>
    <message>
        <source>Unknown or malformed parameter &quot;%1&quot;. Use hwid=…, os=…, osVersion=…, model=… separated by commas.</source>
        <translation>未知或格式错误的参数 &quot;%1&quot;。 使用以逗号间隔的 hwid=…, os=…, osVersion=…, model=… 。</translation>
    </message>
    <message>
        <source>Not a valid Throne backup file.</source>
        <translation>不是有效的 Throne 备份文件。</translation>
    </message>
    <message>
        <source>Unsupported backup format version: %1.
This backup may have been created with a newer version of the application.</source>
        <translation>不支持的备份格式版本: %1。
整个备份可能用该应用程序的较新版本创建。</translation>
    </message>
    <message>
        <source>The backup file is damaged or incomplete.</source>
        <translation>备份文件损坏或不完整。</translation>
    </message>
    <message>
        <source>This backup file does not contain any restorable data.</source>
        <translation>这个备份文件不包含任何可恢复的数据。</translation>
    </message>
    <message>
        <source>The backup&apos;s database is damaged: %1</source>
        <translation>备份的数据库损坏: %1</translation>
    </message>
    <message>
        <source>There is nothing to back up</source>
        <translation>没有要备份的内容</translation>
    </message>
    <message>
        <source>A subscription update is running; try again when it finishes.</source>
        <translation>正在运行订阅更新；待其完成时再试。</translation>
    </message>
    <message>
        <source>Automatic</source>
        <translation>自动</translation>
    </message>
    <message>
        <source>Speed test timeout</source>
        <translation>速度测试超时</translation>
    </message>
    <message>
        <source>URL test timeout</source>
        <translation>URL 测试超时</translation>
    </message>
    <message>
        <source>Mixed inbound</source>
        <translation>混合入站</translation>
    </message>
    <message>
        <source>DNS server</source>
        <translation>DNS 服务器</translation>
    </message>
    <message>
        <source>sing-box API</source>
        <translation>sing-box API</translation>
    </message>
    <message>
        <source>Port %1 is already used by: %2. Choose a different port.</source>
        <translation>端口 %1 已被 %2 使用。选择不同的端口。</translation>
    </message>
    <message>
        <source>NTP server</source>
        <translation>NTP 服务器</translation>
    </message>
    <message>
        <source>NTP port</source>
        <translation>NTP 端口</translation>
    </message>
    <message>
        <source>A geo asset download is already running.</source>
        <translation>地理资源下载已在进行中。</translation>
    </message>
    <message>
        <source>Created by: %1</source>
        <translation>创建者: %1</translation>
    </message>
    <message>
        <source>Backup created on %1.
Select which parts to restore:</source>
        <translation>备份创建在 %1。
选择要恢复哪些部分:</translation>
    </message>
    <message>
        <source>unknown date</source>
        <translation>未知日期</translation>
    </message>
    <message>
        <source>Each selected part replaces the current data. This cannot be undone.
Throne will restart to complete the restore.</source>
        <translation>每个选中部分会替换当前的数据。 这个操作无法撤销。 Throne 将会重启以完成恢复。</translation>
    </message>
    <message>
        <source>Restore</source>
        <translation>恢复</translation>
    </message>
    <message>
        <source>Select at least one part to restore.</source>
        <translation>至少选择一个要恢复的部分。</translation>
    </message>
    <message>
        <source>Enter your WebDAV server in WebDAV Settings first.</source>
        <translation>首先在 WebDAV 设置中输入你的 WebDAV 服务器。</translation>
    </message>
    <message>
        <source>WebDAV Settings</source>
        <translation>WebDAV 设置</translation>
    </message>
    <message>
        <source>Server URL</source>
        <translation>服务器 URL</translation>
    </message>
    <message>
        <source>Username</source>
        <translation>用户名</translation>
    </message>
    <message>
        <source>Password</source>
        <translation>密码</translation>
    </message>
    <message>
        <source>Folder</source>
        <translation>文件夹</translation>
    </message>
    <message>
        <source>Device name</source>
        <translation>设备名称</translation>
    </message>
    <message>
        <source>Connect through the proxy</source>
        <translation>通过代理连接</translation>
    </message>
    <message>
        <source>Test</source>
        <translation>测试</translation>
    </message>
    <message>
        <source>Testing the connection...</source>
        <translation>正在测试连接...</translation>
    </message>
    <message>
        <source>Connected. The backup folder is ready.</source>
        <translation>已连接。备份文件夹已准备就绪。</translation>
    </message>
    <message>
        <source>Connection failed:
%1</source>
        <translation>连接失败:
%1</translation>
    </message>
    <message>
        <source>The server URL must start with http:// or https://.</source>
        <translation>服务器 URL 必须以 http:// 或 https:// 开头。</translation>
    </message>
    <message>
        <source>Uploading the backup...</source>
        <translation>正在上传备份...</translation>
    </message>
    <message>
        <source>Upload failed:
%1</source>
        <translation>上传失败:
%1</translation>
    </message>
    <message>
        <source>Backup uploaded:
%1</source>
        <translation>已上传备份:
%1</translation>
    </message>
    <message>
        <source>Backup uploaded, but the backup list could not be updated: %1</source>
        <translation>备份已上传，但备份列表无法更新: %1</translation>
    </message>
    <message>
        <source>Restore from WebDAV</source>
        <translation>恢复自 WebDAV</translation>
    </message>
    <message>
        <source>Loading the backup list...</source>
        <translation>正在加载备份列表...</translation>
    </message>
    <message>
        <source>Could not list the backups:
%1</source>
        <translation>无法列出备份:
%1</translation>
    </message>
    <message>
        <source>The backup list could not be read: %1</source>
        <translation>无法读取备份列表: %1</translation>
    </message>
    <message>
        <source>There are no backups in the WebDAV folder yet.</source>
        <translation>在 WebDAV 文件夹中尚无备份。</translation>
    </message>
    <message>
        <source>Created</source>
        <translation>创建于</translation>
    </message>
    <message>
        <source>Device</source>
        <translation>设备</translation>
    </message>
    <message>
        <source>Platform</source>
        <translation>平台</translation>
    </message>
    <message>
        <source>Version</source>
        <translation>版本</translation>
    </message>
    <message>
        <source>Parts</source>
        <translation>部分</translation>
    </message>
    <message>
        <source>Size</source>
        <translation>大小</translation>
    </message>
    <message>
        <source>Delete</source>
        <translation>删除</translation>
    </message>
    <message>
        <source>Unknown device</source>
        <translation>未知设备</translation>
    </message>
    <message>
        <source>%1 (this device)</source>
        <translation>%1 (本设备)</translation>
    </message>
    <message>
        <source>Unknown</source>
        <translation>未知</translation>
    </message>
    <message>
        <source>Downloading the backup...</source>
        <translation>正在下载备份...</translation>
    </message>
    <message>
        <source>Could not download the backup:
%1</source>
        <translation>无法下载备份:
%1</translation>
    </message>
    <message>
        <source>Delete Backup</source>
        <translation>删除备份</translation>
    </message>
    <message>
        <source>Delete this backup from the WebDAV server? This cannot be undone.
%1</source>
        <translation>从 WebDAV 服务器删除这个备份吗? 这个无法撤销。
%1</translation>
    </message>
    <message>
        <source>Deleting the backup...</source>
        <translation>正在删除备份...</translation>
    </message>
    <message>
        <source>Could not delete the backup:
%1</source>
        <translation>无法删除备份:
%1</translation>
    </message>
    <message>
        <source>Backup deleted, but the backup list could not be updated: %1</source>
        <translation>备份已删除，但备份列表无法更新: %1</translation>
    </message>
    <message>
        <source>Failed to create temporary file for restore.</source>
        <translation>创建用于恢复的临时文件失败。.</translation>
    </message>
    <message>
        <source>Failed to restore database: %1</source>
        <translation>恢复数据库失败: %1</translation>
    </message>
    <message numerus="yes">
        <source>Skipped %n routing rule(s) that use conditions this version of Throne does not support.</source>
        <translation>
            <numerusform>跳过了 %n 条路由规则，这些规则使用了这一版本的 Throne 不支持的条件。</numerusform>
        </translation>
    </message>
    <message>
        <source>Restore Complete</source>
        <translation>恢复完成</translation>
    </message>
    <message>
        <source>Backup restored successfully. Throne will now restart for the changes to take effect.</source>
        <translation>备份成功恢复。 Throne 现在将要重启以使得更改生效。</translation>
    </message>
    <message>
        <source>Appearance</source>
        <translation>外观</translation>
    </message>
    <message>
        <source>Authentication</source>
        <translation>认证</translation>
    </message>
    <message>
        <source>Auto update</source>
        <translation>自动更新</translation>
    </message>
    <message>
        <source>Backup</source>
        <translation>备份</translation>
    </message>
    <message>
        <source>Certificates</source>
        <translation>证书</translation>
    </message>
    <message>
        <source>Connection</source>
        <translation>连接</translation>
    </message>
    <message>
        <source>Enable connection statistics</source>
        <translation>启用连接统计</translation>
    </message>
    <message>
        <source>Exit country</source>
        <translation>出口国家</translation>
    </message>
    <message>
        <source>General</source>
        <translation>通用</translation>
    </message>
    <message>
        <source>Ignore TLS errors in Throne&apos;s own requests</source>
        <translation>忽略 Throne 自身请求中的 TLS 错误</translation>
    </message>
    <message>
        <source>Latency</source>
        <translation>延迟</translation>
    </message>
    <message>
        <source>Local DNS server</source>
        <translation>本地 DNS 服务器</translation>
    </message>
    <message>
        <source>Local proxy</source>
        <translation>本地代理</translation>
    </message>
    <message>
        <source>Log level</source>
        <translation>日志等级</translation>
    </message>
    <message>
        <source>Log panel</source>
        <translation>日志面板</translation>
    </message>
    <message>
        <source>Logs</source>
        <translation>日志</translation>
    </message>
    <message>
        <source>Privileges</source>
        <translation>权限</translation>
    </message>
    <message>
        <source>Profile list</source>
        <translation>配置列表</translation>
    </message>
    <message>
        <source>Protection</source>
        <translation>防护</translation>
    </message>
    <message>
        <source>Requests</source>
        <translation>请求</translation>
    </message>
    <message>
        <source>Restart proxy when the system proxy is turned off</source>
        <translation>关闭系统代理时重启代理</translation>
    </message>
    <message>
        <source>Send Throne&apos;s own requests through the proxy</source>
        <translation>通过代理发送 Throne 自身的请求</translation>
    </message>
    <message>
        <source>Speed test</source>
        <translation>测速</translation>
    </message>
    <message>
        <source>Statistics</source>
        <translation>统计</translation>
    </message>
    <message>
        <source>Subscriptions</source>
        <translation>订阅</translation>
    </message>
    <message>
        <source>Throne API</source>
        <translation>Throne API</translation>
    </message>
    <message>
        <source>Updates and downloads</source>
        <translation>更新与下载</translation>
    </message>
    <message>
        <source>When updating</source>
        <translation>更新时</translation>
    </message>
    <message>
        <source>Window</source>
        <translation>窗口</translation>
    </message>
</context>
<context>
    <name>DialogEditGroup</name>
    <message>
        <source>Edit Group</source>
        <translation>编辑分组</translation>
    </message>
    <message>
        <source>Common</source>
        <translation>通用</translation>
    </message>
    <message>
        <source>Basic</source>
        <translation>基本</translation>
    </message>
    <message>
        <source>Subscription</source>
        <translation>订阅</translation>
    </message>
    <message>
        <source>Name</source>
        <translation>名称</translation>
    </message>
    <message>
        <source>Landing Proxy</source>
        <translation>落地代理</translation>
    </message>
    <message>
        <source>Type</source>
        <translation>类型</translation>
    </message>
    <message>
        <source>Front Proxy</source>
        <translation>前置代理</translation>
    </message>
    <message>
        <source>Routing Profile</source>
        <translation>路由配置档</translation>
    </message>
    <message>
        <source>The routing profile this group&apos;s profiles connect with. Default follows the routing profile chosen in the Routing menu.</source>
        <translation>这个分组的配置档连接用的路由配置档。“默认”表示遵循「路由」菜单中所选的路由配置档。</translation>
    </message>
    <message>
        <source>Auto Clear Unavailable Profiles</source>
        <translation>自动清除不可用配置档</translation>
    </message>
    <message>
        <source>URL</source>
        <translation>URL</translation>
    </message>
    <message>
        <source>Skip automatic update</source>
        <translation>跳过自动更新</translation>
    </message>
    <message>
        <source>Advanced</source>
        <translation>高级</translation>
    </message>
    <message>
        <source>Each update saves the routing profile the subscription provides (Happ format) as this group&apos;s own routing profile. DNS settings from the subscription are never used.</source>
        <translation>每次更新保存订阅提供的路由配置档（Happ 格式）为这个分组的自有路由配置档。订阅中的 DNS 设置永不采用。</translation>
    </message>
    <message>
        <source>Use the subscription&apos;s routing profile</source>
        <translation>使用订阅的路由配置档</translation>
    </message>
    <message>
        <source>Share</source>
        <translation>分享</translation>
    </message>
    <message>
        <source>Copy profile share links</source>
        <translation>复制配置档分享链接</translation>
    </message>
    <message>
        <source>Copy profile share links (Deep Links)</source>
        <translation>复制配置档分享链接(深度链接)</translation>
    </message>
    <message>
        <source>Default (%1)</source>
        <translation>默认 (%1)</translation>
    </message>
    <message>
        <source>Default</source>
        <translation>默认</translation>
    </message>
    <message>
        <source>%1 (from this subscription)</source>
        <translation>%1 (来自这个订阅)</translation>
    </message>
    <message>
        <source>No profile named &quot;%1&quot;. Pick one from the list.</source>
        <translation>没有名为 &quot;%1&quot; 的配置档。从列表中选取一个。</translation>
    </message>
    <message>
        <source>Copied</source>
        <translation>已复制</translation>
    </message>
    <message>
        <source>None</source>
        <translation>无</translation>
    </message>
    <message>
        <source>INVALID</source>
        <translation>无效</translation>
    </message>
</context>
<context>
    <name>DialogEditGroupAdvanced</name>
    <message>
        <source>Request</source>
        <translation>请求</translation>
    </message>
    <message>
        <source>Empty uses the User Agent from the Subscription settings.</source>
        <translation>留空使用来自「订阅」设置的“用户代理”。</translation>
    </message>
    <message>
        <source>User Agent</source>
        <translation>用户代理标识</translation>
    </message>
    <message>
        <source>Send the HWID, OS, OS version and device model headers with this subscription&apos;s requests. Keep Default follows the Subscription settings.</source>
        <translation>随着这个订阅的请求发送 HWID（硬件ID）、OS（操作系统）、OS 版本和设备信号。保留默认遵循「订阅」设置。</translation>
    </message>
    <message>
        <source>Send HWID</source>
        <translation>发送硬件标识</translation>
    </message>
    <message>
        <source>Keep Default</source>
        <translation>保持默认</translation>
    </message>
    <message>
        <source>Advanced Group Settings</source>
        <translation>高级分组设置</translation>
    </message>
    <message>
        <source>Server Address</source>
        <translation>服务器地址</translation>
    </message>
    <message>
        <source>Host or IP address</source>
        <translation>主机或 IP 地址</translation>
    </message>
    <message>
        <source>TLS versions offered when fetching this subscription. Some servers answer the default handshake with a CAPTCHA or a block page; TLS 1.2 often gets through. Keep Default follows the Subscription settings.</source>
        <translation>获取这个订阅时提供的 TLS 版本。一些服务器会在应答默认握手时返回 CAPTCHA 或拦阻页面；TLS 1.2 通常可以通过。“保持默认”则遵循「订阅」设置。</translation>
    </message>
    <message>
        <source>TLS Version</source>
        <translation>TLS 版本</translation>
    </message>
    <message>
        <source>Auto</source>
        <translation>自动</translation>
    </message>
    <message>
        <source>TLS 1.2</source>
        <translation>TLS 1.2</translation>
    </message>
    <message>
        <source>TLS 1.3</source>
        <translation>TLS 1.3</translation>
    </message>
    <message>
        <source>Auto uses HTTP/2 when the server offers it. Keep Default follows the Subscription settings.</source>
        <translation>当服务器提供时，自动使用 HTTP/2。“保持默认”则遵循「订阅」设置。</translation>
    </message>
    <message>
        <source>HTTP Version</source>
        <translation>HTTP 版本</translation>
    </message>
    <message>
        <source>HTTP/1.1</source>
        <translation>HTTP/1.1</translation>
    </message>
    <message>
        <source>On</source>
        <translation>开启</translation>
    </message>
    <message>
        <source>Off</source>
        <translation>关闭</translation>
    </message>
    <message>
        <source>HWID</source>
        <translation>硬件标识</translation>
    </message>
    <message>
        <source>OS</source>
        <translation>操作系统</translation>
    </message>
    <message>
        <source>OS Version</source>
        <translation>操作系统版本</translation>
    </message>
    <message>
        <source>Device Model</source>
        <translation>设备型号</translation>
    </message>
    <message>
        <source>Update</source>
        <translation>更新</translation>
    </message>
    <message>
        <source>Minutes between automatic updates of this subscription. Keep Default follows the Subscription settings, where automatic updates must be enabled.</source>
        <translation>这个订阅自动更新的间隔分钟数。“保持默认”将遵循「订阅」设置，其中必须启用自动更新。</translation>
    </message>
    <message>
        <source>Update Interval</source>
        <translation>更新间隔</translation>
    </message>
    <message>
        <source>Use the update interval sent by the subscription server; the interval above applies when it sends none. Keep Default follows the Subscription settings.</source>
        <translation>使用由订阅服务器发送的更新间隔；如果服务器未发送则使用上面的间隔。“保持默认”将遵循「订阅」设置。</translation>
    </message>
    <message>
        <source>Server Interval</source>
        <translation>服务器间隔</translation>
    </message>
    <message>
        <source>Use</source>
        <translation>使用</translation>
    </message>
    <message>
        <source>Ignore</source>
        <translation>忽略</translation>
    </message>
    <message>
        <source>Profiles whose last test succeeded are kept when the subscription no longer lists them.</source>
        <translation>上次测试成功的配置档在订阅不再列出它们时仍保留。</translation>
    </message>
    <message>
        <source>Keep working profiles</source>
        <translation>保留工作配置档</translation>
    </message>
    <message>
        <source>After each update:</source>
        <translation>每次更新后:</translation>
    </message>
    <message>
        <source>Remove duplicate profiles</source>
        <translation>移除重复配置档</translation>
    </message>
    <message>
        <source>Remove insecure profiles</source>
        <translation>移除不安全配置档</translation>
    </message>
    <message>
        <source>Profiles the core rejects. Skipped while the core is unreachable.</source>
        <translation>核心拒绝的配置档。当核心不可达时跳过。</translation>
    </message>
    <message>
        <source>Remove invalid profiles</source>
        <translation>移除无效配置档</translation>
    </message>
    <message>
        <source>Tests the group&apos;s profiles once the update finishes, after any test already running.</source>
        <translation>一旦更新完成，在任何已在运行的测试之后，测试该分组的配置档。</translation>
    </message>
    <message>
        <source>Run URL test</source>
        <translation>运行 URL 测试</translation>
    </message>
    <message>
        <source>Remove unavailable profiles</source>
        <translation>移除不可用配置档</translation>
    </message>
    <message>
        <source>Sort by latency</source>
        <translation>按延迟排序</translation>
    </message>
    <message>
        <source>Must be shorter than 1000 characters, on a single line.</source>
        <translation>必须少于 1000 个字符，且在一行上。</translation>
    </message>
    <message>
        <source> min</source>
        <translation> 分</translation>
    </message>
    <message>
        <source>Keep Default (%1)</source>
        <translation>保持默认 (%1)</translation>
    </message>
    <message>
        <source>%1 min</source>
        <translation>%1 分</translation>
    </message>
    <message>
        <source>This server last sent %1 h.</source>
        <translation>这个服务器上次发送时间是 %1 小时前。</translation>
    </message>
    <message>
        <source>Enter a host name or an IP address, without a port.</source>
        <translation>输入一个主机名或 IP 地址，不带端口。</translation>
    </message>
    <message>
        <source>Fixed address</source>
        <translation>固定地址</translation>
    </message>
    <message>
        <source>Must be at least 30 minutes, or 0 to keep the default.</source>
        <translation>必须至少 30 分钟，或者 0 表示保持默认值。</translation>
    </message>
    <message>
        <source>Profiles&apos; own addresses</source>
        <translation>配置档的自身地址</translation>
    </message>
    <message>
        <source>IP list</source>
        <translation>IP 列表</translation>
    </message>
    <message>
        <source>Missing list</source>
        <translation>缺少的列表</translation>
    </message>
    <message>
        <source>Profiles connect to %1 (%2).</source>
        <translation>连接到 %1 的配置档 (%2)。</translation>
    </message>
    <message>
        <source>%1, so profiles use their own addresses.</source>
        <translation>%1，所以配置档使用它们自身的地址。</translation>
    </message>
    <message>
        <source>Filter</source>
        <translation>过滤</translation>
    </message>
    <message>
        <source>Keeps only the profiles whose name matches this regular expression, case-insensitively, each time the subscription is fetched. Empty keeps every profile.</source>
        <translation>每次获取订阅时，仅保留名称匹配此正则表达式的配置档 (不区分大小写)。留空则保留所有配置档。</translation>
    </message>
    <message>
        <source>Keep only matching</source>
        <translation>仅保留匹配项</translation>
    </message>
    <message>
        <source>e.g. HK|Hong Kong</source>
        <translation>例如 HK|香港</translation>
    </message>
    <message>
        <source>Removes the profiles whose name matches this regular expression, case-insensitively, each time the subscription is fetched. It wins over Keep only matching.</source>
        <translation>每次获取订阅时，移除名称匹配此正则表达式的配置档 (不区分大小写)。优先于「仅保留匹配项」。</translation>
    </message>
    <message>
        <source>Remove matching</source>
        <translation>移除匹配项</translation>
    </message>
    <message>
        <source>e.g. expire|traffic</source>
        <translation>例如 到期|流量</translation>
    </message>
    <message>
        <source>Removes every profile of the ticked protocols each time the subscription is fetched.</source>
        <translation>每次获取订阅时，移除所勾选协议的全部配置档。</translation>
    </message>
    <message>
        <source>Remove types</source>
        <translation>移除类型</translation>
    </message>
    <message>
        <source>Exit countries by two-letter code: HK,JP keeps only those, !RU removes one. After each update, profiles whose exit country is unknown get an IP test, and the ones outside these countries are removed. Profiles whose test fails are kept. To filter by a country written in profile names, use Keep only matching.</source>
        <translation>以两字母代码指定出口国家: HK,JP 仅保留这些国家，!RU 移除一个国家。每次更新后，出口国家未知的配置档会进行 IP 测试，不在这些国家中的配置档将被移除。测试失败的配置档会被保留。若要按配置档名称中写明的国家过滤，请使用「仅保留匹配项」。</translation>
    </message>
    <message>
        <source>Countries</source>
        <translation>国家</translation>
    </message>
    <message>
        <source>e.g. HK,JP or !RU</source>
        <translation>例如 HK,JP 或 !RU</translation>
    </message>
    <message>
        <source>None</source>
        <translation>无</translation>
    </message>
    <message>
        <source>Keeps %1.</source>
        <translation>保留 %1。</translation>
    </message>
    <message>
        <source>Removes %1.</source>
        <translation>移除 %1。</translation>
    </message>
    <message numerus="yes">
        <source>After each update, profiles whose exit country is unknown are IP-tested, so the next update can test up to %n profile(s). On a large group this takes a while.</source>
        <translation>
            <numerusform>每次更新后，出口国家未知的配置档会进行 IP 测试，因此下次更新最多可能测试 %n 个配置档。分组较大时需要一段时间。</numerusform>
        </translation>
    </message>
    <message>
        <source>After each update, profiles whose exit country is unknown are IP-tested. On a large subscription this takes a while.</source>
        <translation>每次更新后，出口国家未知的配置档会进行 IP 测试。订阅较大时需要一段时间。</translation>
    </message>
    <message>
        <source>Custom (JSON)</source>
        <translation>自定义 (JSON)</translation>
    </message>
</context>
<context>
    <name>DialogEditIpList</name>
    <message>
        <source>Edit IP list</source>
        <translation>编辑 IP 列表</translation>
    </message>
    <message>
        <source>Name</source>
        <translation>名称</translation>
    </message>
    <message>
        <source>Source</source>
        <translation>来源</translation>
    </message>
    <message>
        <source>Manual</source>
        <translation>手动</translation>
    </message>
    <message>
        <source>URL</source>
        <translation></translation>
    </message>
    <message>
        <source>Rule-set</source>
        <translation>规则集</translation>
    </message>
    <message>
        <source>Auto update</source>
        <translation>自动更新</translation>
    </message>
    <message>
        <source>Enable</source>
        <translation>启用</translation>
    </message>
    <message>
        <source>Every</source>
        <translation>每</translation>
    </message>
    <message>
        <source>minutes</source>
        <translation>分</translation>
    </message>
    <message>
        <source>Entries</source>
        <translation>条目</translation>
    </message>
    <message>
        <source>Fetch now</source>
        <translation>立即获取</translation>
    </message>
    <message>
        <source>One entry per line: 1.2.3.4, 1.2.3.0/24, 1.2.3.4:443, [2606:4700::1]:443, 1.2.3.0/24 443 or 1.2.3.1-1.2.3.9</source>
        <translation>每行一条: 1.2.3.4, 1.2.3.0/24, 1.2.3.4:443, [2606:4700::1]:443, 1.2.3.0/24 443 或 1.2.3.1-1.2.3.9</translation>
    </message>
    <message numerus="yes">
        <source>%Ln unreadable</source>
        <translation>
            <numerusform>%Ln 不可读取</numerusform>
        </translation>
    </message>
    <message>
        <source>Must be from %1 to %2 minutes.</source>
        <translation>必须在 %1 到 %2 分钟之间。</translation>
    </message>
    <message>
        <source>New IP list</source>
        <translation>新建 IP 列表</translation>
    </message>
    <message>
        <source>Enter an http(s) URL.</source>
        <translation>输入一个 http(s) URL。</translation>
    </message>
    <message>
        <source>Choose a rule-set.</source>
        <translation>选择一个规则集。</translation>
    </message>
    <message>
        <source>Unknown rule-set &quot;%1&quot;. Pick a geoip-* name from the list or enter a rule-set URL.</source>
        <translation>未知规则集 &quot;%1&quot;。从列表中选取一个geoip-* 名称或输入一个规则集 URL。</translation>
    </message>
    <message>
        <source>IP list</source>
        <translation>IP 列表</translation>
    </message>
    <message>
        <source>This list no longer exists.</source>
        <translation>这个列表不再存在。</translation>
    </message>
    <message>
        <source>Managed by a scan that no longer exists.</source>
        <translation>由一个不再存在的扫描管理。</translation>
    </message>
    <message>
        <source>Managed by scan %1</source>
        <translation>由扫描 %1 管理</translation>
    </message>
    <message>
        <source>geoip-ir, or a rule-set URL</source>
        <translation>geoip-ir 或一个规则集 URL</translation>
    </message>
    <message>
        <source>Fetching…</source>
        <translation>正在获取...</translation>
    </message>
    <message>
        <source>No entries yet.</source>
        <translation>尚无条目。</translation>
    </message>
    <message numerus="yes">
        <source>%Ln lines</source>
        <translation>
            <numerusform>%Ln 行</numerusform>
        </translation>
    </message>
    <message numerus="yes">
        <source>%Ln entries</source>
        <translation>
            <numerusform>%Ln 条</numerusform>
        </translation>
    </message>
    <message numerus="yes">
        <source>%Ln duplicates</source>
        <translation>
            <numerusform>%Ln 个重复项</numerusform>
        </translation>
    </message>
    <message numerus="yes">
        <source>Fetched %Ln entries</source>
        <translation>
            <numerusform>获取了 %Ln 条</numerusform>
        </translation>
    </message>
    <message numerus="yes">
        <source>showing the first %Ln</source>
        <translation>
            <numerusform>显示前 %Ln</numerusform>
        </translation>
    </message>
    <message>
        <source>The entries are fetched when you press Fetch now or OK.</source>
        <translation>当你按“立即获取”或“确定”时，条目会被获取。</translation>
    </message>
    <message>
        <source>updated %1</source>
        <translation>更新了 %1</translation>
    </message>
    <message>
        <source>last update failed: %1</source>
        <translation>上次更新失败: %1</translation>
    </message>
    <message>
        <source>Fetch failed: %1</source>
        <translation>获取失败: %1</translation>
    </message>
    <message>
        <source>Could not fetch the list:
%1</source>
        <translation>无法获取该列表:
%1</translation>
    </message>
    <message>
        <source>This list is being refreshed. Save again when the refresh has finished.</source>
        <translation>这个列表正在刷新。刷新完成时再次保存。</translation>
    </message>
    <message numerus="yes">
        <source>%Ln entries could not be read and will be dropped.</source>
        <translation>
            <numerusform>%Ln 条无法读取并将被丢弃。</numerusform>
        </translation>
    </message>
    <message>
        <source>For example: %1</source>
        <translation>例如: %1</translation>
    </message>
    <message>
        <source>Save the list without them?</source>
        <translation>不带它们保存列表吗?</translation>
    </message>
    <message>
        <source>Failed to store the IP list.</source>
        <translation>存储 IP 列表失败。</translation>
    </message>
</context>
<context>
    <name>DialogEditOtp</name>
    <message>
        <source>OTP Profile</source>
        <translation>OTP 配置档</translation>
    </message>
    <message>
        <source>Name</source>
        <translation>名称</translation>
    </message>
    <message>
        <source>How you will refer to this OTP</source>
        <translation>你如何称呼这个 OTP</translation>
    </message>
    <message>
        <source>Secret</source>
        <translation>密钥</translation>
    </message>
    <message>
        <source>Base32 secret</source>
        <translation>Base32 密钥</translation>
    </message>
    <message>
        <source>Show secret</source>
        <translation>显示密钥</translation>
    </message>
    <message>
        <source>Issuer</source>
        <translation>颁发者</translation>
    </message>
    <message>
        <source>Optional, kept for import and export</source>
        <translation>可选，保留用于导入和导出</translation>
    </message>
    <message>
        <source>Type</source>
        <translation>类型</translation>
    </message>
    <message>
        <source>TOTP (time based)</source>
        <translation>TOTP (基于时间)</translation>
    </message>
    <message>
        <source>HOTP (counter based)</source>
        <translation>HOTP (基于计数器)</translation>
    </message>
    <message>
        <source>Algorithm</source>
        <translation>算法</translation>
    </message>
    <message>
        <source>SHA1</source>
        <translation>SHA1</translation>
    </message>
    <message>
        <source>SHA256</source>
        <translation>SHA256</translation>
    </message>
    <message>
        <source>SHA512</source>
        <translation>SHA512</translation>
    </message>
    <message>
        <source>Digits</source>
        <translation>位数</translation>
    </message>
    <message>
        <source>Period (seconds)</source>
        <translation>周期 (秒)</translation>
    </message>
    <message>
        <source>Counter</source>
        <translation>计数器</translation>
    </message>
    <message>
        <source>Hide secret</source>
        <translation>隐藏密钥</translation>
    </message>
    <message>
        <source>Not valid base32: use the letters A-Z and the digits 2-7.</source>
        <translation>不是有效的 base32: 使用字母 A-Z 和数字 2-7。</translation>
    </message>
    <message>
        <source>Too short to be a secret.</source>
        <translation>太短无法作为密钥。</translation>
    </message>
    <message>
        <source>Name cannot be empty</source>
        <translation>名称不能为空</translation>
    </message>
    <message>
        <source>Secret is empty</source>
        <translation>密钥是空的</translation>
    </message>
    <message>
        <source>Secret is not valid base32</source>
        <translation>密钥不是有效的 base32</translation>
    </message>
    <message>
        <source>No code yet</source>
        <translation>尚无代码</translation>
    </message>
    <message>
        <source>Current code: %1</source>
        <translation>当前代码: %1</translation>
    </message>
    <message>
        <source>Current code: %1 (%2s)</source>
        <translation>当前代码: %1 (%2s)</translation>
    </message>
</context>
<context>
    <name>DialogEditProfile</name>
    <message>
        <source>Edit</source>
        <translation>编辑</translation>
    </message>
    <message>
        <source>Common</source>
        <translation>通用</translation>
    </message>
    <message>
        <source>Type</source>
        <translation>类型</translation>
    </message>
    <message>
        <source>Port</source>
        <translation>端口</translation>
    </message>
    <message>
        <source>Address</source>
        <translation>地址</translation>
    </message>
    <message>
        <source>Name</source>
        <translation>名称</translation>
    </message>
    <message>
        <source>Advanced Settings</source>
        <translation>高级设置</translation>
    </message>
    <message>
        <source>Settings</source>
        <translation>设置</translation>
    </message>
    <message>
        <source>Network</source>
        <translation>传输(网络)</translation>
    </message>
    <message>
        <source>Security</source>
        <translation>安全</translation>
    </message>
    <message>
        <source>Finalmask</source>
        <translation>最终掩码</translation>
    </message>
    <message>
        <source>Not set</source>
        <translation>未设置</translation>
    </message>
    <message>
        <source>Multiplex</source>
        <translation>多路复用</translation>
    </message>
    <message>
        <source>Keep Default</source>
        <translation>保持默认</translation>
    </message>
    <message>
        <source>Enabled</source>
        <translation>启用</translation>
    </message>
    <message>
        <source>Disabled</source>
        <translation>禁用</translation>
    </message>
    <message>
        <source>Custom</source>
        <translation>自定义</translation>
    </message>
    <message>
        <source>Configure</source>
        <translation>配置</translation>
    </message>
    <message>
        <source>Server support is required</source>
        <translation>需要服务器支持</translation>
    </message>
    <message>
        <source>Transport Layer Security. It must be consistent with the server, otherwise, the connection cannot be established.</source>
        <translation>传输层安全性（安全协议）。它必须与服务器一致，否则无法建立连接。</translation>
    </message>
    <message>
        <source>The underlying transport method. It must be consistent with the server, otherwise, the connection cannot be established.</source>
        <translation>底层传输方式。它必须与服务器一致，否则无法建立连接。</translation>
    </message>
    <message>
        <source>On</source>
        <translation>开启</translation>
    </message>
    <message>
        <source>Off</source>
        <translation>关闭</translation>
    </message>
    <message>
        <source>Network Settings (%1)</source>
        <translation>网络设置 (%1)</translation>
    </message>
    <message>
        <source>&lt;html&gt;&lt;head/&gt;&lt;body&gt;&lt;p&gt;&lt;br/&gt;&lt;/p&gt;&lt;/body&gt;&lt;/html&gt;</source>
        <translation>&lt;html&gt;&lt;head/&gt;&lt;body&gt;&lt;p&gt;&lt;br/&gt;&lt;/p&gt;&lt;/body&gt;&lt;/html&gt;</translation>
    </message>
    <message>
        <source>&lt;html&gt;&lt;head/&gt;&lt;body&gt;&lt;p&gt;Method of http request, will be converted to uppercase&lt;/p&gt;&lt;/body&gt;&lt;/html&gt;</source>
        <translation>&lt;html&gt;&lt;head/&gt;&lt;body&gt;&lt;p&gt;HTTP 请求的方法，将被转换为大写字母&lt;/p&gt;&lt;/body&gt;&lt;/html&gt;</translation>
    </message>
    <message>
        <source>Method</source>
        <translation>方法</translation>
    </message>
    <message>
        <source>Service Name</source>
        <translation>服务名称</translation>
    </message>
    <message>
        <source>TLS Security Settings</source>
        <translation>TLS 安全设置</translation>
    </message>
    <message>
        <source>When enabled, V2Ray will not check the validity of the TLS certificate provided by the remote host (the security is equivalent to plaintext)</source>
        <translation>启用时，V2Ray 将不会检查远端主机提供的 TLS 证书的有效性 (安全性等同于明文)</translation>
    </message>
    <message>
        <source>Allow insecure</source>
        <translation>允许不安全</translation>
    </message>
    <message>
        <source>Certificate</source>
        <translation>证书</translation>
    </message>
    <message>
        <source>Application layer protocol negotiation, clear text. Please separate them with commas.</source>
        <translation>应用层协议协商（明文）。请用英文逗号分隔多项。</translation>
    </message>
    <message>
        <source>Server name indication, clear text.</source>
        <translation>SNI (服务器名称指示)，明文。</translation>
    </message>
    <message>
        <source>TLS Camouflage Settings</source>
        <translation>TLS 伪装设置</translation>
    </message>
    <message>
        <source>Fragment</source>
        <translation>分片</translation>
    </message>
    <message>
        <source>&lt;html&gt;&lt;head/&gt;&lt;body&gt;&lt;p&gt;TLS ClientHello fragmentation. The implementation (built-in / custom) and the &amp;quot;Keep Default&amp;quot; behaviour are configured in Settings &gt; Core.&lt;/p&gt;&lt;/body&gt;&lt;/html&gt;</source>
        <translation>&lt;html&gt;&lt;head/&gt;&lt;body&gt;&lt;p&gt;TLS ClientHello 分片。实现方式 (built-in / custom) 和 &amp;quot;Keep Default&amp;quot; (保持默认)行为在“设置 &gt; 核心”中配置。&lt;/p&gt;&lt;/body&gt;&lt;/html&gt;</translation>
    </message>
    <message>
        <source>&lt;html&gt;&lt;head/&gt;&lt;body&gt;&lt;p&gt;time format, like 500ms, 10ms etc&lt;/p&gt;&lt;/body&gt;&lt;/html&gt;</source>
        <translation>&lt;html&gt;&lt;head/&gt;&lt;body&gt;&lt;p&gt;时间格式，如 500ms, 10ms 等&lt;/p&gt;&lt;/body&gt;&lt;/html&gt;</translation>
    </message>
    <message>
        <source>Fallback Delay</source>
        <translation>回退延时</translation>
    </message>
    <message>
        <source>Enable TLS Record Fragment</source>
        <translation>启用 TLS 记录分片</translation>
    </message>
    <message>
        <source>TLS Tricks</source>
        <translation>TLS 技巧</translation>
    </message>
    <message>
        <source>&lt;html&gt;&lt;head/&gt;&lt;body&gt;&lt;p&gt;Mixed-case SNI to evade exact-match SNI filtering. The &amp;quot;Keep Default&amp;quot; behaviour is configured in Settings &gt; Core.&lt;/p&gt;&lt;/body&gt;&lt;/html&gt;</source>
        <translation>&lt;html&gt;&lt;head/&gt;&lt;body&gt;&lt;p&gt;混合大小写的 SNI 以规避精确匹配的 SNI 过滤。&amp;quot;Keep Default&amp;quot; (保持默认)的行为在“设置 &gt; 核心”中配置。&lt;/p&gt;&lt;/body&gt;&lt;/html&gt;</translation>
    </message>
    <message>
        <source>Cipher Suites</source>
        <translation>密码算法</translation>
    </message>
    <message>
        <source>Colon-separated TLS 1.2 cipher suite names. A uTLS fingerprint ignores them; use the &quot;unsafe&quot; fingerprint (Go&apos;s own TLS stack) to apply them.</source>
        <translation>冒号间隔的 TLS 1.2 密码套件名称。uTLS 指纹会忽略它们，使用“unsafe”（不安全）指纹 (Go 的自有 TLS 栈) 来应用它们。</translation>
    </message>
    <message>
        <source>Reality public key. If not empty, turn TLS into REALITY.</source>
        <translation>Reality 公钥。若不空则 TLS 变为 REALITY。</translation>
    </message>
    <message>
        <source>Reality short id. Accept only one value.</source>
        <translation>Reality SID（短 ID）。 仅接受一个值。</translation>
    </message>
    <message>
        <source>Reality SID</source>
        <translation>Reality SID</translation>
    </message>
    <message>
        <source>Security Settings</source>
        <translation>安全设置</translation>
    </message>
    <message>
        <source>SNI</source>
        <translation>SNI</translation>
    </message>
    <message>
        <source>Fingerprint</source>
        <translation>指纹</translation>
    </message>
    <message>
        <source>ALPN</source>
        <translation>ALPN</translation>
    </message>
    <message>
        <source>Pinned Cert SHA256</source>
        <translation>固定的证书 SHA256</translation>
    </message>
    <message>
        <source>pinnedPeerCertSha256: hex SHA256 of the peer certificate (comma-separated for multiple). Replacement for allowInsecure.</source>
        <translation>pinnedPeerCertSha256：对端证书的十六进制 SHA256 (多个则用逗号间隔)。用于替代 allowInsecure。</translation>
    </message>
    <message>
        <source>Verify Cert Name</source>
        <translation>验证证书名称</translation>
    </message>
    <message>
        <source>verifyPeerCertByName: verify the peer certificate against this name instead of the SNI.</source>
        <translation>verifyPeerCertByName: 针对这个名称而不是 SNI 来验证对端证书。</translation>
    </message>
    <message>
        <source>ECH Config List</source>
        <translation>ECH 配置列表</translation>
    </message>
    <message>
        <source>echConfigList: a base64 ECHConfigList, or &quot;domain+https://dns/dns-query&quot; to let Xray fetch it over DoH (e.g. cloudflare-ech.com+https://8.8.8.8/dns-query).</source>
        <translation>echConfigList: 一个 base64 编码的 ECHConfigList，或者 &quot;domain+https://dns/dns-query&quot; 让 Xray 通过 DoH 获取它 (例如 cloudflare-ech.com+https://8.8.8.8/dns-query)。</translation>
    </message>
    <message>
        <source>cipherSuites: colon-separated TLS 1.2 cipher suite names, including Go&apos;s insecure ones. Only applied with the &quot;unsafe&quot; fingerprint.</source>
        <translation>cipherSuites： 冒号间隔的 TLS 1.2 密码套件名称，包括 Go 的不安全套件。仅配合“unsafe”（不安全）指纹应用。</translation>
    </message>
    <message>
        <source>Reality Pbk</source>
        <translation>Reality 公钥</translation>
    </message>
    <message>
        <source>Reality SpiderX</source>
        <translation>Reality SpiderX</translation>
    </message>
    <message>
        <source>Network Settings</source>
        <translation>网络设置</translation>
    </message>
    <message>
        <source>Host</source>
        <translation>主机</translation>
    </message>
    <message>
        <source>Headers</source>
        <translation>报头</translation>
    </message>
    <message>
        <source>Path</source>
        <translation>路径</translation>
    </message>
    <message>
        <source>EarlyData Length</source>
        <translation>EarlyData 长度</translation>
    </message>
    <message>
        <source>Multi Mode</source>
        <translation>多模式</translation>
    </message>
    <message>
        <source>Mode</source>
        <translation>模式</translation>
    </message>
    <message>
        <source>X Padding Bytes</source>
        <translation>X 填充字节数</translation>
    </message>
    <message>
        <source>Server Max Header Bytes</source>
        <translation>服务器最大标头字节数</translation>
    </message>
    <message>
        <source>Padding Obfuscation</source>
        <translation>填充混淆</translation>
    </message>
    <message>
        <source>xPaddingObfsMode</source>
        <translation>xPaddingObfsMode</translation>
    </message>
    <message>
        <source>xPaddingMethod</source>
        <translation>xPaddingMethod</translation>
    </message>
    <message>
        <source>xPaddingPlacement</source>
        <translation>xPaddingPlacement</translation>
    </message>
    <message>
        <source>xPaddingKey</source>
        <translation>xPaddingKey</translation>
    </message>
    <message>
        <source>xPaddingHeader</source>
        <translation>xPaddingHeader</translation>
    </message>
    <message>
        <source>Upload / Stream Tuning</source>
        <translation>上传/串流调优</translation>
    </message>
    <message>
        <source>scMaxEachPostBytes</source>
        <translation>scMaxEachPostBytes</translation>
    </message>
    <message>
        <source>scMinPostsIntervalMs</source>
        <translation>scMinPostsIntervalMs</translation>
    </message>
    <message>
        <source>scMaxBufferedPosts</source>
        <translation>scMaxBufferedPosts</translation>
    </message>
    <message>
        <source>uplinkHTTPMethod</source>
        <translation>uplinkHTTPMethod</translation>
    </message>
    <message>
        <source>uplinkDataPlacement</source>
        <translation>uplinkDataPlacement</translation>
    </message>
    <message>
        <source>uplinkDataKey</source>
        <translation>uplinkDataKey</translation>
    </message>
    <message>
        <source>uplinkChunkSize</source>
        <translation>uplinkChunkSize</translation>
    </message>
    <message>
        <source>noGRPCHeader</source>
        <translation>noGRPCHeader</translation>
    </message>
    <message>
        <source>noSSEHeader</source>
        <translation>noSSEHeader</translation>
    </message>
    <message>
        <source>scStreamUpServerSecs</source>
        <translation>scStreamUpServerSecs</translation>
    </message>
    <message>
        <source>sessionPlacement</source>
        <translation>sessionPlacement</translation>
    </message>
    <message>
        <source>sessionKey</source>
        <translation>sessionKey</translation>
    </message>
    <message>
        <source>sessionIDTable</source>
        <translation>sessionIDTable</translation>
    </message>
    <message>
        <source>sessionIDLength</source>
        <translation>sessionIDLength</translation>
    </message>
    <message>
        <source>seqPlacement</source>
        <translation>seqPlacement</translation>
    </message>
    <message>
        <source>seqKey</source>
        <translation>seqKey</translation>
    </message>
    <message>
        <source>Download Settings</source>
        <translation>下载设置</translation>
    </message>
    <message>
        <source>Xmux</source>
        <translation>Xmux</translation>
    </message>
    <message>
        <source>maxConcurrency</source>
        <translation>maxConcurrency</translation>
    </message>
    <message>
        <source>maxConnections</source>
        <translation>maxConnections</translation>
    </message>
    <message>
        <source>hMaxRequestTimes</source>
        <translation>hMaxRequestTimes</translation>
    </message>
    <message>
        <source>hMaxReusableSecs</source>
        <translation>hMaxReusableSecs</translation>
    </message>
    <message>
        <source>cMaxReuseTimes</source>
        <translation>cMaxReuseTimes</translation>
    </message>
    <message>
        <source>hKeepAlivePeriod</source>
        <translation>hKeepAlivePeriod</translation>
    </message>
    <message>
        <source>Storage</source>
        <translation>存储</translation>
    </message>
    <message>
        <source>Service</source>
        <translation>服务</translation>
    </message>
    <message>
        <source>Folder</source>
        <translation>文件夹</translation>
    </message>
    <message>
        <source>Client ID</source>
        <translation>客户端ID</translation>
    </message>
    <message>
        <source>Client Secret</source>
        <translation>客户端密钥</translation>
    </message>
    <message>
        <source>Refresh Token</source>
        <translation>刷新令牌</translation>
    </message>
    <message>
        <source>Endpoint</source>
        <translation>端点</translation>
    </message>
    <message>
        <source>Region</source>
        <translation>区域</translation>
    </message>
    <message>
        <source>Bucket</source>
        <translation>桶</translation>
    </message>
    <message>
        <source>Path-Style Addressing</source>
        <translation>路径式寻址</translation>
    </message>
    <message>
        <source>Access Key ID</source>
        <translation>访问钥匙 ID</translation>
    </message>
    <message>
        <source>Secret Access Key</source>
        <translation>加密访问钥匙</translation>
    </message>
    <message>
        <source>Session Token</source>
        <translation>会话令牌</translation>
    </message>
    <message>
        <source>Secrets</source>
        <translation>密钥</translation>
    </message>
    <message>
        <source>Template</source>
        <translation>模板</translation>
    </message>
    <message>
        <source>Front Address</source>
        <translation>前置地址</translation>
    </message>
    <message>
        <source>Front Port</source>
        <translation>前置端口</translation>
    </message>
    <message>
        <source>Tuning</source>
        <translation>调校</translation>
    </message>
    <message>
        <source>Segment Bytes</source>
        <translation>分段字节数</translation>
    </message>
    <message>
        <source>Flush Interval</source>
        <translation>刷新间隔</translation>
    </message>
    <message>
        <source>Poll Interval</source>
        <translation>轮询间隔</translation>
    </message>
    <message>
        <source>Max Poll Interval</source>
        <translation>最大轮询间隔</translation>
    </message>
    <message>
        <source>Eager Window</source>
        <translation>Eager 窗口</translation>
    </message>
    <message>
        <source>Hole Timeout</source>
        <translation>Hole 超时</translation>
    </message>
    <message>
        <source>Session TTL</source>
        <translation>会话 TTL</translation>
    </message>
    <message>
        <source>Concurrency</source>
        <translation>并发数</translation>
    </message>
    <message>
        <source>MTU</source>
        <translation>MTU</translation>
    </message>
    <message>
        <source>TTI</source>
        <translation>TTI</translation>
    </message>
    <message>
        <source>Uplink Capacity</source>
        <translation>上行容量</translation>
    </message>
    <message>
        <source>Downlink Capacity</source>
        <translation>下行容量</translation>
    </message>
    <message>
        <source>CWND Multiplier</source>
        <translation>CWND 倍率</translation>
    </message>
    <message>
        <source>Max Sending Window</source>
        <translation>最大发送窗口</translation>
    </message>
    <message>
        <source>DNSTT / VayDNS</source>
        <translation>DNSTT / VayDNS</translation>
    </message>
    <message>
        <source>Custom (%1 outbound)</source>
        <translation>自定义 (%1 出站)</translation>
    </message>
    <message>
        <source>Custom (%1 config)</source>
        <translation>自定义 (%1 配置)</translation>
    </message>
    <message>
        <source>Custom (Xray outbound)</source>
        <translation>自定义 (Xray 出站)</translation>
    </message>
    <message>
        <source>Custom (Xray config)</source>
        <translation>自定义 (Xray 配置)</translation>
    </message>
    <message>
        <source>Extra Core</source>
        <translation>额外核心</translation>
    </message>
    <message>
        <source>Set by %1.</source>
        <translation>由 %1 设置。</translation>
    </message>
    <message>
        <source>Set by %1; the profile&apos;s own address is %2.</source>
        <translation>由 %1 设置; 该配置档的自有地址是 %2。</translation>
    </message>
    <message>
        <source>This profile was deleted while you were editing it.</source>
        <translation>这个配置档在你编辑时被删除。</translation>
    </message>
    <message>
        <source>Already set</source>
        <translation>已设置</translation>
    </message>
    <message>
        <source>JSON: %1
%2</source>
        <translation>JSON: %1
%2</translation>
    </message>
    <message>
        <source>Xmux Reuse</source>
        <translation>Xmux 重用</translation>
    </message>
    <message>
        <source>XHTTP mode: auto usually uses packet-up, REALITY uses stream-one, and REALITY with downloadSettings uses stream-up. downloadSettings is removed when saving stream-one mode.</source>
        <translation>XHTTP 模式: auto 通常使用 packet-up,  REALITY 使用 stream-one, 而带 downloadSettings 的 REALITY 使用 stream-up。在保存 stream-one 模式时 downloadSettings 会被移除。</translation>
    </message>
    <message>
        <source>Range of extra XHTTP padding bytes. Default: 100-1000. If set, both bounds must be positive.</source>
        <translation>额外 XHTTP 填充字节的范围。默认: 100-1000。 如果设置，则上下限界都必须为正数。</translation>
    </message>
    <message>
        <source>Maximum request header size accepted by the server. Default: 8192.</source>
        <translation>服务器接受的最大请求标头大小。默认: 8192。</translation>
    </message>
    <message>
        <source>Enable Padding Obfuscation</source>
        <translation>启用填充混淆</translation>
    </message>
    <message>
        <source>Enable custom X-Padding placement, key, header, and method. When disabled, the client uses Referer?...x_padding and the server uses X-Padding.</source>
        <translation>启用自定义 X-Padding placement (位置)、key (键名)、header (标头)和 method (方法)。 禁用时，客户端使用 Referer?...x_padding，而服务器使用 X-Padding。</translation>
    </message>
    <message>
        <source>Padding Method</source>
        <translation>填充方法</translation>
    </message>
    <message>
        <source>Padding value format: repeat-x or tokenish. Default: repeat-x.</source>
        <translation>Padding (填充)值格式: repeat-x 或 tokenish 。默认: repeat-x。</translation>
    </message>
    <message>
        <source>Padding Placement</source>
        <translation>填充位置</translation>
    </message>
    <message>
        <source>Where X-Padding is sent: queryInHeader, cookie, header, or query. Default: queryInHeader.</source>
        <translation>发送 X-Padding 的位置: queryInHeader, cookie, header 或 query。默认: queryInHeader。</translation>
    </message>
    <message>
        <source>Padding Key</source>
        <translation>填充键名</translation>
    </message>
    <message>
        <source>Query or cookie key for X-Padding, and query key inside queryInHeader. Default: x_padding.</source>
        <translation>X-Padding 使用的查询或 cookie 键，以及 queryInHeader 内的查询参数键。默认: x_padding.</translation>
    </message>
    <message>
        <source>Padding Header</source>
        <translation>填充标头</translation>
    </message>
    <message>
        <source>Header name used by header or queryInHeader padding. Default: X-Padding.</source>
        <translation>header 或 queryInHeader padding (填充)使用的标头名称。默认: X-Padding。</translation>
    </message>
    <message>
        <source>Max Post Bytes</source>
        <translation>最大 Post 字节数</translation>
    </message>
    <message>
        <source>Packet-up upload POST size: client split size and server reject limit. Default: 1000000.</source>
        <translation>Packet-up (上行数据包)模式的上传 POST 大小: 客户端分片大小 (client split size)和服务器拒绝上限 (server reject limit)。 默认: 1000000。</translation>
    </message>
    <message>
        <source>Min Post Interval</source>
        <translation>最小 Post 间隔</translation>
    </message>
    <message>
        <source>Packet-up client interval between upload POST requests per proxied connection, in milliseconds. Default: 30.</source>
        <translation>Packet-up (上行数据)模式下，每个代理的连接的上传 POST 请求之间的客户端间隔时间，以毫秒为单位。 默认: 30。</translation>
    </message>
    <message>
        <source>Max Buffered Posts</source>
        <translation>最大缓冲的 POST 请求数</translation>
    </message>
    <message>
        <source>Packet-up server upload queue size per proxied connection. Default: 30.</source>
        <translation>Packet-up (上行数据)模式下，每个代理的连接的服务器上传队列大小。 默认: 30。</translation>
    </message>
    <message>
        <source>Uplink HTTP Method</source>
        <translation>上行 HTTP 方法</translation>
    </message>
    <message>
        <source>HTTP method for upload requests. Default: POST. Xray uppercases it; GET is accepted only in packet-up mode.</source>
        <translation>上传请求使用的 HTTP 方法。 默认： POST 。Xray 会把它转为大写； 仅在 Packet-up 模式中才接受 GET 。</translation>
    </message>
    <message>
        <source>Uplink Data Placement</source>
        <translation>上行数据位置</translation>
    </message>
    <message>
        <source>Where upload data is placed. Default: auto. cookie/header are accepted only in packet-up mode; auto/body are always accepted.</source>
        <translation>放置上传数据的位置。 默认： auto 。 cookie/header 仅在 packet-up 模式中被接受； auto/body 则始终被接受。</translation>
    </message>
    <message>
        <source>Uplink Data Key</source>
        <translation>上行数据键</translation>
    </message>
    <message>
        <source>Key used when upload data is placed in a cookie or header. Defaults: X-Data for auto/header, x_data for cookie.</source>
        <translation>当上传数据被放在 cookie 或 header 中时使用的键。 默认： X-Data 用于 auto/header, x_data 用于 cookie。</translation>
    </message>
    <message>
        <source>Uplink Chunk Size</source>
        <translation>上行分块大小</translation>
    </message>
    <message>
        <source>Packet-up header/cookie payload chunk size range. Defaults: cookie 2-3 KiB, header 3-4 KB, otherwise scMaxEachPostBytes. Values below 64 are clamped.</source>
        <translation>Packet-up 中 header/cookie 的载荷(payload)分块大小范围。默认： cookie 为 2-3 KiB， header  为 3-4 KB，其他情况为 scMaxEachPostBytes。 低于 64 的值会被按 64 处理。</translation>
    </message>
    <message>
        <source>No GRPC Headers</source>
        <translation>无 GRPC 标头</translation>
    </message>
    <message>
        <source>Client-side stream-up/stream-one option: do not add Content-Type: application/grpc to upload requests.</source>
        <translation>客户端的 stream-up/stream-one 选项： 不向上传请求添加 Content-Type: application/grpc 。</translation>
    </message>
    <message>
        <source>No SSE Headers</source>
        <translation>无 SSE 标头</translation>
    </message>
    <message>
        <source>Server-side downstream/stream-one option: do not send Content-Type: text/event-stream in responses.</source>
        <translation>服务器端 downstream/stream-one 选项： 不在响应中发送 Content-Type: text/event-stream 。</translation>
    </message>
    <message>
        <source>Stream Up Server Seconds</source>
        <translation>Stream-up (上行流)服务器秒数</translation>
    </message>
    <message>
        <source>Stream-up server interval for periodic xPaddingBytes keepalive writes, in seconds. Default: 20-80; values &lt;= 0 disable periodic padding.</source>
        <translation>Stream-up (上行流)模式下，服务器定期写入 xPaddingBytes 的 keepalive (保活)数据的时间间隔, 以秒为单位。 默认： 20-80； 不大于 0 的值禁止周期性填充。</translation>
    </message>
    <message>
        <source>Session Placement</source>
        <translation>会话位置</translation>
    </message>
    <message>
        <source>Where the XHTTP session id is sent: path, cookie, header, or query. Default: path.</source>
        <translation>发送 XHTTP session id (会话ID) 的位置： path, cookie, header 或 query。默认： path。</translation>
    </message>
    <message>
        <source>Session Key</source>
        <translation>会话键</translation>
    </message>
    <message>
        <source>Key used for the session id outside path placement. Defaults: x_session for cookie/query, X-Session for header.</source>
        <translation>在 path 外用于放置会话 ID 的键。默认： x_session 用于 cookie/query, X-Session 用于 header。</translation>
    </message>
    <message>
        <source>Session ID Table</source>
        <translation>会话 ID 表</translation>
    </message>
    <message>
        <source>Charset for generating the XHTTP session id: a predefined name (number, hex, HEX, base36, BASE36, alphabet, ALPHABET, Alphabet, Base62) or a literal ASCII string. Empty falls back to a random UUID.</source>
        <translation>用于生成 XHTTP 会话 Id 的字符集： 一个预定义的名称(数字, hex, HEX, base36, BASE36, alphabet, ALPHABET, Alphabet, Base62) 或一个字面的 ASCII 字符串。空白则回退为一个随机 UUID。</translation>
    </message>
    <message>
        <source>Session ID Length</source>
        <translation>会话 ID 长度</translation>
    </message>
    <message>
        <source>Length range of the generated session id, e.g. 8-16. Only used together with sessionIDTable; &quot;from&quot; must be greater than 0.</source>
        <translation>生成的会话 ID 的长度范围，例如 8-16。仅配合 sessionIDTable 一起使用； “from” (从)必须大于 0。</translation>
    </message>
    <message>
        <source>Sequence Placement</source>
        <translation>序列位置</translation>
    </message>
    <message>
        <source>Where the XHTTP packet sequence is sent: path, cookie, header, or query. Default: path.</source>
        <translation>发送 XHTTP 数据包序列的位置： path, cookie, header 或 query。 默认： path。</translation>
    </message>
    <message>
        <source>Sequence Key</source>
        <translation>序列键</translation>
    </message>
    <message>
        <source>Key used for the sequence value outside path placement. Defaults: x_seq for cookie/query, X-Seq for header.</source>
        <translation>在 path (路径)外用于放置序列值的键。 默认： x_seq 用于 cookie/query， X-Seq 用于 header。</translation>
    </message>
    <message>
        <source>Max Concurrency</source>
        <translation>最大并发数</translation>
    </message>
    <message>
        <source>Client-side H2/H3 xmux limit: maximum concurrent uses per underlying connection. Cannot be used together with maxConnections. Empty xmux defaults to 1-1.</source>
        <translation>客户端侧 H2/H3 xmux 限制： 每个底层连接使用的最大并发用量。 不能与 maxConnections 一起使用。空 xmux 则默认为 1-1。</translation>
    </message>
    <message>
        <source>Max Connections</source>
        <translation>最大连接数</translation>
    </message>
    <message>
        <source>Client-side H2/H3 xmux limit: maximum parallel underlying connections. Cannot be used together with maxConcurrency.</source>
        <translation>客户端侧 H2/H3 xmux 限制： 最大并行的底层连接数。 不能与 maxConcurrency 一起使用。</translation>
    </message>
    <message>
        <source>Max Reuse times</source>
        <translation>最大复用次数</translation>
    </message>
    <message>
        <source>Client-side H2/H3 xmux limit: maximum times an underlying connection may be selected for reuse.</source>
        <translation>客户端侧 H2/H3 xmux 限制： 单个底层连接可被选择重复使用的最大次数。</translation>
    </message>
    <message>
        <source>Max Request Times</source>
        <translation>最大请求次数</translation>
    </message>
    <message>
        <source>Client-side H2/H3 xmux limit: maximum upload/download requests per underlying connection. Empty xmux defaults to 600-900.</source>
        <translation>客户端侧 H2/H3 xmux 限制： 每个底层连接的最大 upload/download (上传/下载)请求数。 空 xmux 默认为 600-900。</translation>
    </message>
    <message>
        <source>Max Reusable Secs</source>
        <translation>最大可重用秒数</translation>
    </message>
    <message>
        <source>Client-side H2/H3 xmux limit: maximum seconds an underlying connection stays reusable. Empty xmux defaults to 1800-3000.</source>
        <translation>客户端侧 H2/H3 xmux 限制： 一个底层连接保持可复用的最多秒数。 空 xmux 则默认为 1800-3000。</translation>
    </message>
    <message>
        <source>Keep Alive Period</source>
        <translation>保活周期</translation>
    </message>
    <message>
        <source>Client-side H2/H3 keepalive interval for underlying connections, in seconds. 0 uses Xray defaults; negative values disable keepalive where supported.</source>
        <translation>客户端侧底层 H2/H3 连接的 keepalive (保持活动)间隔，以秒为单位。 0 使用 Xray 默认值； 负值在支持的情况下会禁用保活功能。</translation>
    </message>
    <message>
        <source>Client-only downstream streamSettings, including address and port, for an independent download path. Not allowed in stream-one and removed when saving stream-one mode.</source>
        <translation>仅客户端使用的 downstream (下行) streamSettings，包括独立下载路径的地址和端口。在 stream-one 中不允许使用该选项，并在保存 stream-one 模式时会被移除该选项。</translation>
    </message>
    <message>
        <source>The padding method or placement is not supported, and Xray refuses it even with obfuscation off. Turn this on and fix the value, or clear it.</source>
        <translation>padding （填充）方法或 placement （位置）不被支持，而且即使关闭混淆 Xray 也拒绝它。打开它并修正该值，或者清除它。</translation>
    </message>
    <message>
        <source>Largest packet mKCP sends, in bytes, from 21 to 8192. Default: 1350.</source>
        <translation>mKCP 发送的最大数据包大小（字节），范围从 21 到 8192。默认值：1350。</translation>
    </message>
    <message>
        <source>How often mKCP sends, in milliseconds, from 10 to 1000. Default: 50.</source>
        <translation>mKCP 的发送间隔（毫秒），范围从 10 到 1000。默认值：50。</translation>
    </message>
    <message>
        <source>Most data sent per second, in MB/s; 0 means very little. Default: 5.</source>
        <translation>每秒发送的数据量上限（MB/s）；0 表示发送量很少。默认值：5。</translation>
    </message>
    <message>
        <source>Most data received per second, in MB/s; 0 means very little. Default: 20.</source>
        <translation>每秒接收的数据量上限（MB/s）；0 表示接收量很少。默认值：20。</translation>
    </message>
    <message>
        <source>Multiplies how many packets may be in flight at once. Default: 1.</source>
        <translation>一次性可在途的数据包数量（CWND 拥塞窗口）乘以的倍率。默认值：1。</translation>
    </message>
    <message>
        <source>Most data waiting to be sent or acknowledged, in bytes; at least the MTU. Default: 2097152.</source>
        <translation>等待发送或等待确认的数据量上限（字节）；该值至少应为 MTU。默认值：2097152。</translation>
    </message>
    <message>
        <source>Resolvers</source>
        <translation>解析器</translation>
    </message>
    <message>
        <source>The short ID has no effect without a Reality public key, so Reality stays off.</source>
        <translation>没有 Reality 公钥，短 ID 没有效果，所以 Reality 保持关闭。</translation>
    </message>
    <message>
        <source>Where both ends exchange the stream: google-drive uses a Google Drive folder, s3 a bucket of any S3-compatible storage, template any HTTP storage API described by Template, local a folder on this machine.</source>
        <translation>两端交换数据流的位置：google-drive 使用 Google Drive 文件夹；s3 使用任意兼容 S3 存储服务中的存储桶；template 使用由 Template 描述的任意 HTTP 存储 API；local 使用本机上的文件夹。</translation>
    </message>
    <message>
        <source>google-drive: the folder ID. s3: the key prefix inside the bucket. local: a directory path. template: read as {folder}.</source>
        <translation>google-drive: 文件夹 ID。 s3: 存储桶内的钥匙前缀。 local: 目录路径。 template: 按 {folder} 读取。</translation>
    </message>
    <message>
        <source>The Google Drive OAuth client ID.</source>
        <translation>Google Drive OAuth 客户端 ID。</translation>
    </message>
    <message>
        <source>The Google Drive OAuth client secret.</source>
        <translation>Google Drive OAuth 客户端密钥。</translation>
    </message>
    <message>
        <source>The OAuth refresh token for the Google account that owns the folder.</source>
        <translation>OAuth 刷新拥有该文件夹的 Google 账户的令牌。</translation>
    </message>
    <message>
        <source>The S3 API URL, such as https://s3.us-east-1.amazonaws.com or a MinIO or R2 endpoint.</source>
        <translation>S3 API URL，譬如 https://s3.us-east-1.amazonaws.com 或一个 MinIO 或 R2 端点。</translation>
    </message>
    <message>
        <source>The region requests are signed for. Default: us-east-1.</source>
        <translation>用于签署请求的区域。默认：us-east-1。</translation>
    </message>
    <message>
        <source>The bucket both ends share.</source>
        <translation>两端共享的存储桶。</translation>
    </message>
    <message>
        <source>Puts the bucket in the URL path. When off, the bucket becomes part of the endpoint&apos;s host name, which needs a DNS endpoint and a DNS-compatible bucket name. Default: on.</source>
        <translation>放置存储桶名称在 URL 路径中。关闭这个选项时，存储桶会成为端点的主机名的一部分，这要求端点是 DNS 地址，且存储桶名称兼容 DNS 命名规则。默认：开启。</translation>
    </message>
    <message>
        <source>The S3 access key ID.</source>
        <translation>S3 访问钥匙 ID。</translation>
    </message>
    <message>
        <source>The S3 secret access key.</source>
        <translation>S3 加密访问钥匙。</translation>
    </message>
    <message>
        <source>Optional: the session token of temporary credentials.</source>
        <translation>可选：临时凭证的会话令牌。</translation>
    </message>
    <message>
        <source>Values the template reads as {secret0}, {secret1}, …, separated by commas.</source>
        <translation>各值 template（模板）读取为 {secret0}, {secret1}, …, 用逗号间隔。</translation>
    </message>
    <message>
        <source>The storage API&apos;s auth, put, get, list and delete operations, as JSON.</source>
        <translation>存储 API 的 auth, put, get, list 和 delete 操作，以  JSON 方式。</translation>
    </message>
    <message>
        <source>Optional domain front: the storage API is dialed at this address while TLS still names the API host.</source>
        <translation>可选域名前置：在 TLS 仍命名 API 主机的情况下，存储 API 会通过这个地址拨号。</translation>
    </message>
    <message>
        <source>Port of the front address. Default: 443.</source>
        <translation>前置地址的端口。默认值：443。</translation>
    </message>
    <message>
        <source>Largest upload per file, in bytes. Default: 524288; Xray caps it at 16 MiB.</source>
        <translation>每个文件的最大上传大小（字节）。默认值：524288；Xray 限制它为 16 MiB。</translation>
    </message>
    <message>
        <source>How long writes are gathered before an upload, in milliseconds. Default: 20.</source>
        <translation>上传前，写入会被收集多长时间，以毫秒为单位。默认值：20。</translation>
    </message>
    <message>
        <source>Shortest wait between checks for new files, in milliseconds. Default: 50.</source>
        <translation>检查新文件之间的最短等待时间，毫秒为单位。默认值：50。</translation>
    </message>
    <message>
        <source>Longest wait between checks while the stream is idle, in milliseconds. Default: 500.</source>
        <translation>流闲置时两次检查间的最长等待时间，毫秒为单位。默认值：500。</translation>
    </message>
    <message>
        <source>How long after traffic polling stays at the shortest wait, in milliseconds. Default: 2000.</source>
        <translation>流量轮询在最短等待时间保持多久，毫秒为单位。默认值：2000。</translation>
    </message>
    <message>
        <source>How long a missing segment is waited for before the stream fails, in milliseconds. Default: 30000.</source>
        <translation>（认为）流失败前，等待丢失片段的时间长度，毫秒为单位。默认值：30000。</translation>
    </message>
    <message>
        <source>How long an idle session lives before its files are cleaned up, in seconds. Default: 300.</source>
        <translation>一个空闲会话在其文件被清理前能存活多久，秒为单位。默认值：300。</translation>
    </message>
    <message>
        <source>Parallel storage requests per stream. Default: 8; Xray caps it at 64.</source>
        <translation>每个流的并行存储请求。默认值：8；Xray 限制在 64。</translation>
    </message>
    <message>
        <source>Google Drive folder ID</source>
        <translation>Google Drive 文件夹 ID</translation>
    </message>
    <message>
        <source>Key prefix in the bucket</source>
        <translation>存储桶中的钥匙前缀</translation>
    </message>
    <message>
        <source>A directory path</source>
        <translation>目录路径</translation>
    </message>
    <message>
        <source>&lt;html&gt;&lt;head/&gt;&lt;body&gt;&lt;p&gt;http path (ws/http/httpUpgrade)&lt;/p&gt;&lt;p&gt;serviceName (gRPC)&lt;/p&gt;&lt;/body&gt;&lt;/html&gt;</source>
        <translation>&lt;html&gt;&lt;head/&gt;&lt;body&gt;&lt;p&gt;HTTP 路径 (ws/http/httpUpgrade)&lt;/p&gt;&lt;p&gt;serviceName (gRPC)&lt;/p&gt;&lt;/body&gt;&lt;/html&gt;</translation>
    </message>
    <message>
        <source>EarlyData Name</source>
        <translation>EarlyData 名称</translation>
    </message>
    <message>
        <source>&lt;html&gt;&lt;head/&gt;&lt;body&gt;&lt;p&gt;Headers in space separated key value pair:&lt;/p&gt;&lt;p&gt;key1=val1 key2=&amp;quot;multi word val2&amp;quot; &amp;quot;multi word key3&amp;quot;=val3&lt;/p&gt;&lt;/body&gt;&lt;/html&gt;</source>
        <translation>&lt;html&gt;&lt;head/&gt;&lt;body&gt;&lt;p&gt;以空格分隔的键值对形式的报头：&lt;/p&gt;&lt;p&gt;key1=val1 key2=&amp;quot;multi word val2&amp;quot; &amp;quot;multi word key3&amp;quot;=val3&lt;/p&gt;&lt;/body&gt;&lt;/html&gt;</translation>
    </message>
    <message>
        <source>&lt;html&gt;&lt;head/&gt;&lt;body&gt;&lt;p&gt;http host (ws/http/httpUpgrade)&lt;/p&gt;&lt;/body&gt;&lt;/html&gt;</source>
        <translation>&lt;html&gt;&lt;head/&gt;&lt;body&gt;&lt;p&gt;HTTP 主机 (ws/http/httpUpgrade)&lt;/p&gt;&lt;/body&gt;&lt;/html&gt;</translation>
    </message>
</context>
<context>
    <name>DialogEndpointDetails</name>
    <message>
        <source>Endpoint Details</source>
        <translation>端点细节</translation>
    </message>
    <message>
        <source>Endpoint — %1</source>
        <translation>端点 — %1</translation>
    </message>
    <message>
        <source>State</source>
        <translation>状态</translation>
    </message>
    <message>
        <source>Error</source>
        <translation>错误</translation>
    </message>
    <message>
        <source>Server</source>
        <translation>服务器</translation>
    </message>
    <message>
        <source>Transport</source>
        <translation>传输</translation>
    </message>
    <message>
        <source>Cipher</source>
        <translation>密码算法</translation>
    </message>
    <message>
        <source>MTU</source>
        <translation>MTU</translation>
    </message>
    <message>
        <source>Connected for</source>
        <translation>连接时长</translation>
    </message>
    <message>
        <source>IPv4 address</source>
        <translation>IPv4 地址</translation>
    </message>
    <message>
        <source>IPv6 address</source>
        <translation>IPv6 地址</translation>
    </message>
    <message>
        <source>All domains — every DNS query goes through this tunnel</source>
        <translation>所有域名 — 所有 DNS 查询均通过这个隧道</translation>
    </message>
    <message>
        <source>Routes through this tunnel</source>
        <translation>通过这个隧道路由</translation>
    </message>
    <message>
        <source>Kept outside this tunnel</source>
        <translation>保持在这个隧道外</translation>
    </message>
    <message>
        <source>DNS servers pushed by the server</source>
        <translation>由服务器推送的 DNS 服务器</translation>
    </message>
    <message>
        <source>Domains routed to this tunnel</source>
        <translation>路由转发到这个隧道的域名</translation>
    </message>
    <message>
        <source>No longer running</source>
        <translation>不再运行</translation>
    </message>
</context>
<context>
    <name>DialogIntegration</name>
    <message>
        <source>Integration Settings</source>
        <translation>集成设置</translation>
    </message>
    <message>
        <source>Global Hotkeys</source>
        <translation>全局热键</translation>
    </message>
    <message>
        <source>Trigger main window</source>
        <translation>显示/隐藏主窗口</translation>
    </message>
    <message>
        <source>Show groups</source>
        <translation>显示分组</translation>
    </message>
    <message>
        <source>Show routes</source>
        <translation>显示路由</translation>
    </message>
    <message>
        <source>Proxy mode</source>
        <translation>代理模式</translation>
    </message>
    <message>
        <source>Toggle System Proxy</source>
        <translation>切换系统代理</translation>
    </message>
    <message>
        <source>Start/Stop Profile</source>
        <translation>启动/停止配置档</translation>
    </message>
    <message>
        <source>Toggle Tun Mode</source>
        <translation>切换 Tun 模式</translation>
    </message>
    <message>
        <source>Shortcuts</source>
        <translation>快捷键</translation>
    </message>
    <message>
        <source>Throne API</source>
        <translation>Throne API</translation>
    </message>
    <message>
        <source>Enable the Throne API</source>
        <translation>启用 Throne API</translation>
    </message>
    <message>
        <source>Listen on</source>
        <translation>监听</translation>
    </message>
    <message>
        <source>This computer only</source>
        <translation>仅本电脑</translation>
    </message>
    <message>
        <source>Local network</source>
        <translation>本地网络</translation>
    </message>
    <message>
        <source>Port</source>
        <translation>端口</translation>
    </message>
    <message>
        <source>Allowed addresses</source>
        <translation>允许的地址</translation>
    </message>
    <message>
        <source>Any address, or e.g. 192.168.1.0/24, 10.0.0.5</source>
        <translation>任何地址，或者如 192.168.1.0/24, 10.0.0.5</translation>
    </message>
    <message>
        <source>Lets devices on the local network import, delete, change settings, back up and restore through the Throne API. This device can always do so.</source>
        <translation>让本地网络上的设备通过 Throne API 导入、删除、更改设置、备份和恢复。这个设备始终可以这样做。</translation>
    </message>
    <message>
        <source>Allow management from the local network</source>
        <translation>运行从本地网络管理</translation>
    </message>
    <message>
        <source>Key</source>
        <translation>键</translation>
    </message>
    <message>
        <source>Show</source>
        <translation>显示</translation>
    </message>
    <message>
        <source>Copy</source>
        <translation>复制</translation>
    </message>
    <message>
        <source>Regenerate</source>
        <translation>重新生成</translation>
    </message>
    <message>
        <source>Copy for scripts</source>
        <translation>为脚本复制</translation>
    </message>
    <message>
        <source>Status</source>
        <translation>状态</translation>
    </message>
    <message>
        <source>Devices on your network can control Throne with this key. Your firewall may ask to allow Throne.</source>
        <translation>你的网络上的设备可以用这个钥匙控制 Throne。 你的防火墙可能要求允许 Throne。</translation>
    </message>
    <message>
        <source>Global hotkeys can only be a single key combination.</source>
        <translation>全局热键只可以是单键组合。</translation>
    </message>
    <message>
        <source>&quot;%1&quot; needs Ctrl, Alt or Meta, otherwise it would block typing in other applications.</source>
        <translation>&quot;%1&quot; 需要 Ctrl, Alt 或 Meta，否则它会阻止在其它应用程序中输入。</translation>
    </message>
    <message>
        <source>Enter an IP address or a range such as 192.168.1.0/24 or fd00::/8.</source>
        <translation>输入一个 IP 地址或一个范围，例如 192.168.1.0/24 或 fd00::/8。</translation>
    </message>
    <message>
        <source>&quot;%1&quot; is also assigned to &quot;%2&quot;.</source>
        <translation>&quot;%1&quot; 也被分配给 &quot;%2&quot;。</translation>
    </message>
    <message>
        <source>Scripts connect with thronectl or the Throne API library.</source>
        <translation>脚本用 thronectl 或 Throne API 库进行连接。</translation>
    </message>
    <message>
        <source>Hide</source>
        <translation>隐藏</translation>
    </message>
    <message>
        <source>Regenerate Key</source>
        <translation>重新生成钥匙</translation>
    </message>
    <message>
        <source>Generate a new key?</source>
        <translation>生成新的钥匙吗?</translation>
    </message>
    <message>
        <source>Scripts using the current key will stop working.</source>
        <translation>使用当前钥匙的脚本将停止工作。</translation>
    </message>
    <message>
        <source>Hotkey</source>
        <translation>热键</translation>
    </message>
    <message>
        <source>These global hotkeys could not be registered:</source>
        <translation>这些全局热键无法被注册:</translation>
    </message>
    <message>
        <source>127.0.0.1 (default)</source>
        <translation>127.0.0.1 (默认)</translation>
    </message>
    <message>
        <source>Clash API</source>
        <translation>Clash API</translation>
    </message>
    <message>
        <source>Core APIs</source>
        <translation>核心 API</translation>
    </message>
    <message>
        <source>DNS server</source>
        <translation>DNS 服务器</translation>
    </message>
    <message>
        <source>Listen Address</source>
        <translation>监听地址</translation>
    </message>
    <message>
        <source>Listen Port</source>
        <translation>监听端口</translation>
    </message>
    <message>
        <source>Loopback only. Leave the port empty to keep the web dashboard off.</source>
        <translation>仅限 Loopback (回环)访问。 端口留空表示关闭Web仪表盘。</translation>
    </message>
    <message>
        <source>Mixed inbound</source>
        <translation>混合入站</translation>
    </message>
    <message>
        <source>Port %1 is already used by: %2. Choose a different port.</source>
        <translation>端口 %1 已被 %2 使用。选择不同的端口。</translation>
    </message>
    <message>
        <source>Regenerate Secret</source>
        <translation>重新生成密钥</translation>
    </message>
    <message>
        <source>Secret</source>
        <translation>密钥</translation>
    </message>
    <message>
        <source>sing-box API</source>
        <translation>sing-box API</translation>
    </message>
    <message>
        <source>sing-box API / Dashboard</source>
        <translation>sing-box API / 仪表盘</translation>
    </message>
</context>
<context>
    <name>DialogIpLists</name>
    <message>
        <source>IP Lists</source>
        <translation>IP 列表</translation>
    </message>
    <message>
        <source>New</source>
        <translation>新建</translation>
    </message>
    <message>
        <source>Import</source>
        <translation>导入</translation>
    </message>
    <message>
        <source>Duplicate</source>
        <translation>复制</translation>
    </message>
    <message>
        <source>Export</source>
        <translation>导出</translation>
    </message>
    <message numerus="yes">
        <source>…and %Ln more</source>
        <translation>
            <numerusform>...还有 %Ln</numerusform>
        </translation>
    </message>
    <message>
        <source>From text…</source>
        <translation>来自文本...</translation>
    </message>
    <message>
        <source>From file…</source>
        <translation>来自文件...</translation>
    </message>
    <message>
        <source>From URL…</source>
        <translation>来自 URL...</translation>
    </message>
    <message>
        <source>From rule-set…</source>
        <translation>来自规则集...</translation>
    </message>
    <message>
        <source>Add default lists</source>
        <translation>添加默认列表</translation>
    </message>
    <message>
        <source>Every default list is already present.</source>
        <translation>每个默认列表都已存在。</translation>
    </message>
    <message>
        <source>Importing…</source>
        <translation>正在导入...</translation>
    </message>
    <message>
        <source>Delete IP list</source>
        <translation>删除 IP 列表</translation>
    </message>
    <message>
        <source>&quot;%1&quot; is being refreshed. Delete it when the refresh has finished.</source>
        <translation>&quot;%1&quot; 正在被刷新。当刷新已完成时删除它。</translation>
    </message>
    <message>
        <source>Delete the IP list &quot;%1&quot;?</source>
        <translation>删除 IP 列表 &quot;%1&quot; 吗?</translation>
    </message>
    <message>
        <source>Scan &quot;%1&quot; is running and stores its results in this list. Pause it first.</source>
        <translation>扫描 &quot;%1&quot; 正在运行并在这个列表中存储其结果。首先暂停它。</translation>
    </message>
    <message>
        <source>It holds the results of scan &quot;%1&quot;, which creates a new result list the next time it runs.</source>
        <translation>它存放了扫描“%1”的结果，下次运行时会创建一个新的结果列表。</translation>
    </message>
    <message>
        <source>These scans use it as their targets: %1</source>
        <translation>这些扫描使用它作为它们的目标: %1</translation>
    </message>
    <message>
        <source>%1 copy</source>
        <translation>%1 副本</translation>
    </message>
    <message>
        <source>Duplicate IP list</source>
        <translation>重复 IP 列表</translation>
    </message>
    <message>
        <source>Name of the copy:</source>
        <translation>副本的名称:</translation>
    </message>
    <message>
        <source>Failed to copy the IP list.</source>
        <translation>复制 IP 列表失败。</translation>
    </message>
    <message>
        <source>Export IP list</source>
        <translation>导出 IP 列表</translation>
    </message>
    <message>
        <source>Text files (*.txt);;All files (*)</source>
        <translation>文本文件 (*.txt);;所有文件 (*)</translation>
    </message>
    <message>
        <source>Cannot write to: %1</source>
        <translation>无法写到: %1</translation>
    </message>
    <message numerus="yes">
        <source>Exported %Ln entries to:
%1</source>
        <translation>
            <numerusform>导出了 %Ln 条到:
%1</numerusform>
        </translation>
    </message>
    <message>
        <source>Import IP lists</source>
        <translation>导入 IP 列表</translation>
    </message>
    <message>
        <source>All files (*);;Text and CSV (*.txt *.csv *.lst *.list);;sing-box rule-sets (*.srs *.json)</source>
        <translation>所有文件 (*);;文本和 CSV (*.txt *.csv *.lst *.list);;sing-box 规则集 (*.srs *.json)</translation>
    </message>
    <message>
        <source>%1: cannot be opened</source>
        <translation>%1: 无法打开</translation>
    </message>
    <message>
        <source>%1: larger than 50 MB, skipped</source>
        <translation>%1: 大于 50 MB，已跳过</translation>
    </message>
    <message>
        <source>%1: no addresses found</source>
        <translation>%1: 未找到地址</translation>
    </message>
    <message>
        <source>%1: could not be stored</source>
        <translation>%1: 无法被存储</translation>
    </message>
    <message numerus="yes">
        <source>%1: %Ln unreadable entries skipped</source>
        <translation>
            <numerusform>%1: 跳过了 %Ln 个不可读取的条目</numerusform>
        </translation>
    </message>
    <message>
        <source>Nothing could be imported:</source>
        <translation>没有可被导入的内容:</translation>
    </message>
    <message numerus="yes">
        <source>Imported %Ln list(s)</source>
        <translation>
            <numerusform>导入了 %Ln 个列表</numerusform>
        </translation>
    </message>
    <message numerus="yes">
        <source>with %Ln entries.</source>
        <translation>
            <numerusform>带 %Ln 项。</numerusform>
        </translation>
    </message>
</context>
<context>
    <name>DialogManageGroups</name>
    <message>
        <source>Groups</source>
        <translation>分组</translation>
    </message>
    <message>
        <source>New group</source>
        <translation>新建分组</translation>
    </message>
    <message>
        <source>Update all subscriptions</source>
        <translation>更新所有订阅</translation>
    </message>
    <message>
        <source>Confirmation</source>
        <translation>确认</translation>
    </message>
    <message>
        <source>Update all subscriptions?</source>
        <translation>更新所有订阅吗？</translation>
    </message>
</context>
<context>
    <name>DialogManageRoutes</name>
    <message>
        <source>&lt;html&gt;&lt;head/&gt;&lt;body&gt;&lt;p&gt;Used as default strategy when resolving outbound/dns server Domain addresses&lt;/p&gt;&lt;/body&gt;&lt;/html&gt;</source>
        <translation>&lt;html&gt;&lt;head/&gt;&lt;body&gt;&lt;p&gt;解析出站/DNS 服务器域地址时，作为默认策略使用&lt;/p&gt;&lt;/body&gt;&lt;/html&gt;</translation>
    </message>
    <message>
        <source>Default Domain Strategy</source>
        <translation>默认域策略</translation>
    </message>
    <message>
        <source>Remote Rule-set Mirror</source>
        <translation>远程规则集镜像</translation>
    </message>
    <message>
        <source>&lt;html&gt;&lt;head/&gt;&lt;body&gt;&lt;p&gt;If not empty, resolves domains in requests according to the strategy&lt;/p&gt;&lt;/body&gt;&lt;/html&gt;</source>
        <translation>&lt;html&gt;&lt;head/&gt;&lt;body&gt;&lt;p&gt;如果不为空，则根据策略解析请求中的域&lt;/p&gt;&lt;/body&gt;&lt;/html&gt;</translation>
    </message>
    <message>
        <source>Resolve Domain Strategy</source>
        <translation>解析域策略</translation>
    </message>
    <message>
        <source>Routing Profile</source>
        <translation>路由配置档</translation>
    </message>
    <message>
        <source>GitHub</source>
        <translation>GitHub</translation>
    </message>
    <message>
        <source>jsDelivr(Cloudflare)</source>
        <translation>jsDelivr(Cloudflare)</translation>
    </message>
    <message>
        <source>jsDelivr(Gcore)</source>
        <translation>jsDelivr(Gcore)</translation>
    </message>
    <message>
        <source>jsDelivr(Quantil)</source>
        <translation>jsDelivr(Quantil)</translation>
    </message>
    <message>
        <source>jsDelivr(Fastly)</source>
        <translation>jsDelivr(Fastly)</translation>
    </message>
    <message>
        <source>jsDelivr(CDN)</source>
        <translation>jsDelivr(CDN)</translation>
    </message>
    <message>
        <source>&lt;html&gt;&lt;head/&gt;&lt;body&gt;&lt;p&gt;Enables the following config to be used as egress, useful for bypassing services restrciting non-warp VPNs&lt;/p&gt;&lt;/body&gt;&lt;/html&gt;</source>
        <translation>&lt;html&gt;&lt;head/&gt;&lt;body&gt;&lt;p&gt;启用以下配置作为出口(egress)使用，对于绕过限制非 warp VPNs 的服务很有用&lt;/p&gt;&lt;/body&gt;&lt;/html&gt;</translation>
    </message>
    <message>
        <source>WireGuard runs over UDP. AmneziaWG adds WARP-compatible junk and camouflage packets. MASQUE tunnels over HTTP/3 or HTTP/2.</source>
        <translation>WireGuard 经 UDP 运行。 AmneziaWG 添加兼容 WARP 的填充流量和伪装数据包。MASQUE 隧道则经由 HTTP/3 或 HTTP/2。</translation>
    </message>
    <message>
        <source>Endpoint</source>
        <translation>端点</translation>
    </message>
    <message>
        <source>Private Key</source>
        <translation>私钥</translation>
    </message>
    <message>
        <source>Public Key</source>
        <translation>公钥</translation>
    </message>
    <message>
        <source>&lt;html&gt;&lt;head/&gt;&lt;body&gt;&lt;p&gt;Gets a new config from cloudflare API&lt;/p&gt;&lt;/body&gt;&lt;/html&gt;</source>
        <translation>&lt;html&gt;&lt;head/&gt;&lt;body&gt;&lt;p&gt;从 cloudflare API 获取一个新配置&lt;/p&gt;&lt;/body&gt;&lt;/html&gt;</translation>
    </message>
    <message>
        <source>Generate Warp Config</source>
        <translation>生成 Warp 配置</translation>
    </message>
    <message>
        <source>Reserved</source>
        <translation>保留</translation>
    </message>
    <message>
        <source>Mode</source>
        <translation>模式</translation>
    </message>
    <message>
        <source>Interface Addresses</source>
        <translation>接口地址</translation>
    </message>
    <message>
        <source>Base64 DER of the server&apos;s public key. When set, it pins the server certificate.</source>
        <translation>服务器公钥的 Base64 DER 编码。设置时，它会固定服务器证书。</translation>
    </message>
    <message>
        <source>Peer Public Key</source>
        <translation>对端公钥</translation>
    </message>
    <message>
        <source>HTTP/3 runs over UDP; HTTP/2 runs over TLS on TCP and works where UDP is blocked.</source>
        <translation>HTTP/3 基于 UDP 运行；HTTP/2 基于 TCP 上的 TLS 运行并在 UDP 被阻断的情况下也可工作。</translation>
    </message>
    <message>
        <source>HTTP Version</source>
        <translation>HTTP 版本</translation>
    </message>
    <message>
        <source>HTTP/3 (fallback to HTTP/2)</source>
        <translation>HTTP/3 (回退到 HTTP/2)</translation>
    </message>
    <message>
        <source>HTTP/3 only</source>
        <translation>仅 HTTP/3</translation>
    </message>
    <message>
        <source>Cloudflare API domains to try, in order, when generating a WARP config</source>
        <translation>当生成 WARP 配置时，按顺序尝试的 Cloudflare API 域名</translation>
    </message>
    <message>
        <source>Registration Domains…</source>
        <translation>注册域名...</translation>
    </message>
    <message>
        <source>DNS</source>
        <translation>DNS</translation>
    </message>
    <message>
        <source>&lt;html&gt;&lt;head/&gt;&lt;body&gt;&lt;p&gt;Local DNS can not be used in some configurations, you will have to set an ip here to be used as local dns in such situations&lt;/p&gt;&lt;/body&gt;&lt;/html&gt;</source>
        <translation>&lt;html&gt;&lt;head/&gt;&lt;body&gt;&lt;p&gt;在某些配置中无法使用本地 DNS，在这种情况下您必须在这里设置一个 IP 作为本地 DNS 使用&lt;/p&gt;&lt;/body&gt;&lt;/html&gt;</translation>
    </message>
    <message>
        <source>Local Override</source>
        <translation>本地覆盖</translation>
    </message>
    <message>
        <source>&lt;html&gt;&lt;head/&gt;&lt;body&gt;&lt;p&gt;When enabled, domains destined for direct outbound will also be resolved using direct dns&lt;/p&gt;&lt;/body&gt;&lt;/html&gt;</source>
        <translation>&lt;html&gt;&lt;head/&gt;&lt;body&gt;&lt;p&gt;启用时，目标为直接出站的域也将使用直接 DNS 进行解析&lt;/p&gt;&lt;/body&gt;&lt;/html&gt;</translation>
    </message>
    <message>
        <source>Enable DNS Routing</source>
        <translation>启用 DNS 路由</translation>
    </message>
    <message>
        <source>&lt;html&gt;&lt;head/&gt;&lt;body&gt;&lt;p&gt;Answer A/AAAA queries from the system hosts file before consulting any DNS server. Only domains actually listed in the file are affected.&lt;/p&gt;&lt;/body&gt;&lt;/html&gt;</source>
        <translation>&lt;html&gt;&lt;head/&gt;&lt;body&gt;&lt;p&gt;在查询任何 DNS 服务器前，先从系统 hosts 文件回答 A/AAAA 查询。只有该文件中实际列出的域名会受到影响。&lt;/p&gt;&lt;/body&gt;&lt;/html&gt;</translation>
    </message>
    <message>
        <source>Respect Hosts File</source>
        <translation>尊重 Hosts 文件</translation>
    </message>
    <message>
        <source>Predefined Answers</source>
        <translation>预定义回答</translation>
    </message>
    <message>
        <source>&lt;html&gt;&lt;head/&gt;&lt;body&gt;&lt;p&gt;examples:&lt;br/&gt;tls://8.8.8.8&lt;br/&gt;https://domain/path&lt;/p&gt;&lt;p&gt;tcp://8.8.8.8:1234&lt;/p&gt;&lt;p&gt;dhcp://auto&lt;/p&gt;&lt;p&gt;h3://domain/path&lt;/p&gt;&lt;p&gt;quic://domain:4632&lt;/p&gt;&lt;p&gt;etc&lt;/p&gt;&lt;/body&gt;&lt;/html&gt;</source>
        <translation>&lt;html&gt;&lt;head/&gt;&lt;body&gt;&lt;p&gt;示例:&lt;br/&gt;tls://8.8.8.8&lt;br/&gt;https://domain/path&lt;/p&gt;&lt;p&gt;tcp://8.8.8.8:1234&lt;/p&gt;&lt;p&gt;dhcp://auto&lt;/p&gt;&lt;p&gt;h3://domain/path&lt;/p&gt;&lt;p&gt;quic://domain:4632&lt;/p&gt;&lt;p&gt;等&lt;/p&gt;&lt;/body&gt;&lt;/html&gt;</translation>
    </message>
    <message>
        <source>Remote DNS</source>
        <translation>远程 DNS</translation>
    </message>
    <message>
        <source>Answer AAAA queries sent to this server with an empty result.</source>
        <translation>用空结果回答发送到这个服务器的 AAAA 查询。</translation>
    </message>
    <message>
        <source>Disable IPv6</source>
        <translation>禁用 IPv6</translation>
    </message>
    <message>
        <source>This is especially important and it is recommended to use the default value of &quot;localhost&quot;.
If the default value does not work, try changing it to &quot;223.5.5.5&quot;.
For more information, see the document &quot;Configuration/DNS&quot;.</source>
        <translation>这个尤其重要，建议使用默认值 &quot;localhost&quot;。
如果默认值不起作用，请尝试改为 &quot;223.5.5.5&quot;。
更多信息，请参阅文档 &quot;配置/DNS&quot;。</translation>
    </message>
    <message>
        <source>Direct DNS</source>
        <translation>直连 DNS</translation>
    </message>
    <message>
        <source>Default DNS server</source>
        <translation>默认 DNS 服务器</translation>
    </message>
    <message>
        <source>Enable FakeIP</source>
        <translation>启用 FakeIP</translation>
    </message>
    <message>
        <source>Hand out no fake IPv6 addresses; AAAA queries are answered empty.</source>
        <translation>不发出伪 IPv6 地址； AAAA 查询会被回答空结果。</translation>
    </message>
    <message>
        <source>FakeIP Disable IPv6</source>
        <translation>FakeIP 禁用 IPv6</translation>
    </message>
    <message>
        <source>Advanced</source>
        <translation>高级</translation>
    </message>
    <message>
        <source>&lt;html&gt;&lt;head/&gt;&lt;body&gt;&lt;p&gt;Cache behaviour, optimistic cache and query timeout.&lt;/p&gt;&lt;/body&gt;&lt;/html&gt;</source>
        <translation>&lt;html&gt;&lt;head/&gt;&lt;body&gt;&lt;p&gt;缓存行为，乐观缓存和查询超时。&lt;/p&gt;&lt;/body&gt;&lt;/html&gt;</translation>
    </message>
    <message>
        <source>Advanced Settings</source>
        <translation>高级设置</translation>
    </message>
    <message>
        <source>Use Custom DNS Object</source>
        <translation>使用自定义 DNS 对象</translation>
    </message>
    <message>
        <source>Edit DNS Object</source>
        <translation>编辑 DNS 对象</translation>
    </message>
    <message>
        <source>Routing Profiles</source>
        <translation>路由配置档</translation>
    </message>
    <message>
        <source>New</source>
        <translation>新建</translation>
    </message>
    <message>
        <source>Clone</source>
        <translation>克隆</translation>
    </message>
    <message>
        <source>Export</source>
        <translation>导出</translation>
    </message>
    <message>
        <source>Import</source>
        <translation>导入</translation>
    </message>
    <message>
        <source>Edit</source>
        <translation>编辑</translation>
    </message>
    <message>
        <source>Delete</source>
        <translation>删除</translation>
    </message>
    <message>
        <source>Update the selected remote routing profile, or all of them</source>
        <translation>更新选定的远程路由配置档，或它们全部</translation>
    </message>
    <message>
        <source>Update</source>
        <translation>更新</translation>
    </message>
    <message>
        <source>WARP Registration Domains</source>
        <translation>WARP 注册域名</translation>
    </message>
    <message>
        <source>One domain per line. They are tried in order and the first one that accepts the registration is used. Leave empty to use %1.</source>
        <translation>每行一个域名。它们将按顺序尝试并使用第一个接受注册的域名。留空则使用 %1。</translation>
    </message>
    <message>
        <source>Invalid state</source>
        <translation>无效状态</translation>
    </message>
    <message>
        <source>The list of routing profiles is empty, this should be an unreachable state, crashes may occur now</source>
        <translation>路由配置档的列表为空，这应该是一个不可达的状态，现在可能会发生崩溃</translation>
    </message>
    <message>
        <source>Predefined DNS Answers</source>
        <translation>预定义的 DNS 回答</translation>
    </message>
    <message>
        <source>Enable predefined answers</source>
        <translation>启用预定义回答</translation>
    </message>
    <message>
        <source>One entry per line, hosts-file syntax:
127.0.0.1 localhost
10.0.0.5 nas.lan files.lan
::1 localhost6
A domain listed with only one address family is answered NXDOMAIN for the other, so the override cannot be bypassed.</source>
        <translation>每行一条记录，hosts 文件语法:
127.0.0.1 localhost
10.0.0.5 nas.lan files.lan
::1 localhost6
一个仅配置了一个地址族的域名，对于其它的地址族会回答 NXDOMAIN (Non-Existent Domain 不存在域名)，因此无法绕过该覆盖设置。</translation>
    </message>
    <message>
        <source>Invalid input</source>
        <translation>无效输入</translation>
    </message>
    <message>
        <source>Advanced DNS Settings</source>
        <translation>高级 DNS 设置</translation>
    </message>
    <message>
        <source>Cache Capacity</source>
        <translation>缓存容量</translation>
    </message>
    <message>
        <source>10s (default)</source>
        <translation>10s (10秒，默认)</translation>
    </message>
    <message>
        <source>Query Timeout</source>
        <translation>查询超时</translation>
    </message>
    <message>
        <source>Optimistic Cache</source>
        <translation>乐观缓存</translation>
    </message>
    <message>
        <source>&lt;html&gt;&lt;head/&gt;&lt;body&gt;&lt;p&gt;Keep serving an expired answer while it is refreshed in the background. Cannot be combined with Disable Cache or Disable Expire.&lt;/p&gt;&lt;/body&gt;&lt;/html&gt;</source>
        <translation>&lt;html&gt;&lt;head/&gt;&lt;body&gt;&lt;p&gt;后台刷新的同时，持续提供已过期的应答。 不能与“禁用缓存”或“禁用有效期”一起使用。&lt;/p&gt;&lt;/body&gt;&lt;/html&gt;</translation>
    </message>
    <message>
        <source>3d (default)</source>
        <translation>3d (3天，默认)</translation>
    </message>
    <message>
        <source>Optimistic Timeout</source>
        <translation>乐观超时</translation>
    </message>
    <message>
        <source>Disable Cache</source>
        <translation>禁用缓存</translation>
    </message>
    <message>
        <source>Disable Expire</source>
        <translation>禁用有效期</translation>
    </message>
    <message>
        <source>Save Cache To File</source>
        <translation>保存缓存到文件</translation>
    </message>
    <message>
        <source>&lt;html&gt;&lt;head/&gt;&lt;body&gt;&lt;p&gt;Write cached DNS answers and FakeIP mappings to the core&apos;s cache file so they survive a restart. Off by default: each entry costs a disk write.&lt;/p&gt;&lt;/body&gt;&lt;/html&gt;</source>
        <translation>&lt;html&gt;&lt;head/&gt;&lt;body&gt;&lt;p&gt;将缓存的 DNS 应答和 FakeIP 映射写到核心的缓存文件，以便在重启后仍然存在。默认关闭：每个条目都会耗费一次磁盘写入。&lt;/p&gt;&lt;/body&gt;&lt;/html&gt;</translation>
    </message>
    <message>
        <source>Reverse Mapping</source>
        <translation>反向映射</translation>
    </message>
    <message>
        <source>Invalid settings</source>
        <translation>无效设置</translation>
    </message>
    <message>
        <source>DNS Object</source>
        <translation>DNS 对象</translation>
    </message>
    <message>
        <source>Format</source>
        <translation>格式化</translation>
    </message>
    <message>
        <source>The DNS object is not a valid JSON object</source>
        <translation>DNS 对象不是有效的 JSON 对象</translation>
    </message>
    <message>
        <source>Document</source>
        <translation>文档</translation>
    </message>
    <message>
        <source>Generating config...</source>
        <translation>正在生成配置…</translation>
    </message>
    <message>
        <source>Failed to generate warp config</source>
        <translation>生成 warp 配置失败</translation>
    </message>
    <message>
        <source>Success!</source>
        <translation>成功!</translation>
    </message>
    <message>
        <source>Routing profile cannot be empty</source>
        <translation>路由配置档不能为空</translation>
    </message>
    <message>
        <source>It uses its own DNS servers: remote %1, direct %2.</source>
        <translation>它使用自己的 DNS 服务器：远程 %1，直连 %2。</translation>
    </message>
    <message>
        <source>Use an IP address, for example 223.5.5.5 or tls://1.1.1.1: this server resolves the names of the others.</source>
        <translation>使用一个 IP 地址，例如 223.5.5.5 或 tls://1.1.1.1：这个服务器解析其他服务器的名称。</translation>
    </message>
    <message>
        <source>Use three numbers from 0 to 255, for example 12,34,56.</source>
        <translation>使用三个从 0 到 255 的数字，例如12,34,56。</translation>
    </message>
    <message>
        <source>&quot;%1&quot; is not an IP address.</source>
        <translation>&quot;%1&quot; 不是 IP 地址。</translation>
    </message>
    <message>
        <source>Add at least one host name after the address, for example 10.0.0.5 nas.lan.</source>
        <translation>在地址后至少添加一个主机名，例如 10.0.0.5 nas.lan。</translation>
    </message>
    <message>
        <source>&quot;%1&quot; is not a host name.</source>
        <translation>&quot;%1&quot; 不是一个主机名。</translation>
    </message>
    <message>
        <source>Line %1: &quot;%2&quot; never matches: %3</source>
        <translation>第 %1 行： &quot;%2&quot; 从未匹配： %3</translation>
    </message>
    <message>
        <source>Put an IPv6 address in brackets, for example [2606:4700::1].</source>
        <translation>放置 IPv6 地址在方括号里，例如 [2606:4700::1]。</translation>
    </message>
    <message>
        <source>%1 (updated by subscription &quot;%2&quot;)</source>
        <translation>%1 (由订阅 &quot;%2&quot; 更新)</translation>
    </message>
    <message>
        <source>Every update of the subscription &quot;%1&quot; overwrites its rules. Deleting it stops the import.</source>
        <translation>订阅 &quot;%1&quot; 的每次更新都会覆盖其规则。删除它即停止导入。</translation>
    </message>
    <message>
        <source>%1 (default)</source>
        <translation>%1 (默认)</translation>
    </message>
    <message>
        <source>The core raises values below 1024 to 1024.</source>
        <translation>核心会把低于1024的值提高到1024。</translation>
    </message>
    <message>
        <source>It is on but empty: click Edit DNS Object to write one, or turn it off.</source>
        <translation>它是开启状态，但为空：点击【编辑 DNS 对象】来写一个，或关闭它。</translation>
    </message>
    <message>
        <source>The object is empty: add at least its &quot;servers&quot;, or turn it off.</source>
        <translation>该对象为空：至少添加它的“服务器”，或关闭它。</translation>
    </message>
    <message>
        <source>Required while WARP is on: click Generate Warp Config, or fill it in.</source>
        <translation>WARP 开启时必需：点击【生成 WARP 配置】，或手动填写。</translation>
    </message>
    <message>
        <source>WARP has no config yet: click Generate Warp Config.</source>
        <translation>WARP 尚无配置: 点击【生成 Warp 配置】。</translation>
    </message>
    <message>
        <source>Generating the AmneziaWG QUIC signature...</source>
        <translation>正在生成 AmneziaWG QUIC 签名...</translation>
    </message>
    <message>
        <source>AmneziaWG has no QUIC signature yet: click Generate Warp Config.</source>
        <translation>AmneziaWG 尚无 QUIC 签名，点击“生成 Warp 配置”。</translation>
    </message>
    <message>
        <source>Structured profile</source>
        <translation>结构化配置档</translation>
    </message>
    <message>
        <source>Raw profile</source>
        <translation>原始配置档</translation>
    </message>
    <message>
        <source>Remote profile</source>
        <translation>远程配置档</translation>
    </message>
    <message>
        <source>Exported with warnings</source>
        <translation>带警告导出</translation>
    </message>
    <message>
        <source>Copied!</source>
        <translation>已复制!</translation>
    </message>
    <message>
        <source>Add remote routing profiles</source>
        <translation>添加远程路由配置档</translation>
    </message>
    <message>
        <source>No valid remote routing profiles in the link.</source>
        <translation>链接中无有效的远程路由配置档。</translation>
    </message>
    <message>
        <source>Add these remote routing profiles?</source>
        <translation>添加这些远程路由配置档吗?</translation>
    </message>
    <message>
        <source>auto update</source>
        <translation>自动更新</translation>
    </message>
    <message>
        <source>On</source>
        <translation>开启</translation>
    </message>
    <message>
        <source>Off</source>
        <translation>关闭</translation>
    </message>
    <message>
        <source>a routing rule list</source>
        <translation>路由规则列表</translation>
    </message>
    <message>
        <source>routing profile &quot;%1&quot;</source>
        <translation>路由配置档 &quot;%1&quot;</translation>
    </message>
    <message>
        <source>Import %1 from the clipboard?</source>
        <translation>从剪贴板中导入 %1 吗?</translation>
    </message>
    <message>
        <source>Note:</source>
        <translation>备注:</translation>
    </message>
    <message>
        <source>Import from clipboard</source>
        <translation>从剪贴板导入</translation>
    </message>
    <message>
        <source>Import routing profile</source>
        <translation>导入路由配置档</translation>
    </message>
    <message>
        <source>Paste a Throne route link, a remoteRoute link, a base64 blob, or a JSON rule array</source>
        <translation>粘贴一个 Throne 路由链接，一个 remoteRoute 链接，一个 base64 blob 或者一个 JSON 规则数组</translation>
    </message>
    <message>
        <source>Could not import this routing profile:
</source>
        <translation>无法导入这个路由配置档:
</translation>
    </message>
    <message>
        <source>Imported with warnings</source>
        <translation>带警告导入</translation>
    </message>
    <message>
        <source>Invalid operation</source>
        <translation>无效操作</translation>
    </message>
    <message>
        <source>Routing Profiles cannot be empty, try adding another profile or editing this one</source>
        <translation>路由配置档不能为空，请尝试添加另一个配置档或编辑这个配置档</translation>
    </message>
    <message>
        <source>Cancel</source>
        <translation>取消</translation>
    </message>
    <message>
        <source>Cancelling...</source>
        <translation>正在取消...</translation>
    </message>
    <message>
        <source>Update selected</source>
        <translation>更新选定项</translation>
    </message>
    <message>
        <source>Update all</source>
        <translation>更新全部</translation>
    </message>
    <message>
        <source>No remote profiles</source>
        <translation>无远程路由配置档</translation>
    </message>
    <message>
        <source>There are no remote routing profiles to update.</source>
        <translation>没有要更新的远程路由配置档。</translation>
    </message>
    <message>
        <source>Updating...</source>
        <translation>正在更新...</translation>
    </message>
    <message>
        <source>Updating (%1 / %2)</source>
        <translation>正在更新 (%1 / %2)</translation>
    </message>
    <message numerus="yes">
        <source>%n profile(s) were updated with warnings; see the details.</source>
        <translation>
            <numerusform>%n 个配置档被更新，但带警告；查阅详情。</numerusform>
        </translation>
    </message>
    <message>
        <source>Update cancelled</source>
        <translation>更新已取消</translation>
    </message>
    <message>
        <source>Cancelled: updated %1 of %2, %3 failed.</source>
        <translation>已取消: 更新了 %1 / %2 个,失败了 %3 个。</translation>
    </message>
    <message>
        <source>Update complete</source>
        <translation>更新完成</translation>
    </message>
    <message>
        <source>Updated %1 remote routing profile(s).</source>
        <translation>更新了 %1 个远程路由配置档。</translation>
    </message>
    <message>
        <source>Update finished with errors</source>
        <translation>带错误完成更新</translation>
    </message>
    <message>
        <source>Updated %1, failed %2:
%3</source>
        <translation>更新了 %1, 失败了 %2:
%3</translation>
    </message>
    <message>
        <source>TLS fragmentation for WARP registration</source>
        <translation>WARP 注册时的 TLS 分片</translation>
    </message>
    <message>
        <source>Advanced…</source>
        <translation>高级…</translation>
    </message>
    <message>
        <source>Advanced WARP Settings</source>
        <translation>高级 WARP 设置</translation>
    </message>
    <message>
        <source>Registration</source>
        <translation>注册</translation>
    </message>
    <message>
        <source>Applies only when registration connects directly, not through a running profile.</source>
        <translation>仅在注册直接连接、而非经由正在运行的配置档时生效。</translation>
    </message>
    <message>
        <source>Fragment TLS</source>
        <translation>TLS 分片</translation>
    </message>
    <message>
        <source>Record fragment</source>
        <translation>记录分片</translation>
    </message>
    <message>
        <source>Fallback delay</source>
        <translation>回退延时</translation>
    </message>
    <message>
        <source>Domain Strategy</source>
        <translation>域名策略</translation>
    </message>
    <message>
        <source>Egress</source>
        <translation>出口</translation>
    </message>
    <message>
        <source>Enable</source>
        <translation>启用</translation>
    </message>
    <message>
        <source>Exit through</source>
        <translation>出口方式</translation>
    </message>
    <message>
        <source>Interval (minute, invalid if less than 30)</source>
        <translation>时间间隔 (分钟，少于 30 则无效)</translation>
    </message>
    <message>
        <source>Resolution</source>
        <translation>解析</translation>
    </message>
    <message>
        <source>Routing</source>
        <translation>路由</translation>
    </message>
    <message>
        <source>Routing Settings</source>
        <translation>路由设置</translation>
    </message>
    <message>
        <source>Routing profiles auto update</source>
        <translation>路由配置档自动更新</translation>
    </message>
    <message>
        <source>Selected server</source>
        <translation>所选服务器</translation>
    </message>
    <message>
        <source>Servers</source>
        <translation>服务器</translation>
    </message>
    <message>
        <source>Traffic leaves through the server you started.</source>
        <translation>流量从你启动的服务器出去。</translation>
    </message>
    <message>
        <source>Updates</source>
        <translation>更新</translation>
    </message>
    <message>
        <source>WARP runs over the selected server.</source>
        <translation>WARP 运行在所选服务器之上。</translation>
    </message>
</context>
<context>
    <name>DialogNewScan</name>
    <message>
        <source>Scan %1</source>
        <translation>扫描 %1</translation>
    </message>
    <message>
        <source>New scan</source>
        <translation>新建扫描</translation>
    </message>
    <message>
        <source>Name</source>
        <translation>名称</translation>
    </message>
    <message>
        <source>Generic scan</source>
        <translation>通用扫描</translation>
    </message>
    <message>
        <source>WARP scan</source>
        <translation>WARP 扫描</translation>
    </message>
    <message>
        <source>DNS resolver scan</source>
        <translation>DNS 解析器扫描</translation>
    </message>
    <message>
        <source>Probe addresses with ping, TCP, TLS or HTTP, and optionally test them through one of your profiles.</source>
        <translation>用 ping、TCP、TLS 或 HTTP 探测地址，并可选择通过你的其中一个配置档测试它们。</translation>
    </message>
    <message>
        <source>Find Cloudflare WARP endpoints that complete a WireGuard, AmneziaWG or MASQUE connection.</source>
        <translation>查找可以完成 WireGuard、AmneziaWG 或 MASQUE 连接的 Cloudflare WARP 端点。</translation>
    </message>
    <message>
        <source>Find DNS resolvers that relay the tunnel of one of your DNSTT, MasterDNS or XDNS profiles.</source>
        <translation>查找可转发你的 DNSTT、MasterDNS 或 XDNS 配置档之一的隧道流量的 DNS 解析器。</translation>
    </message>
    <message>
        <source>Failed to store the scan.</source>
        <translation>存储该扫描失败。</translation>
    </message>
    <message>
        <source>Find working settings</source>
        <translation>查找可用设置</translation>
    </message>
    <message>
        <source>Try variants of one profile&apos;s anti-censorship settings several times each and rank them; nothing changes in the profile until you apply a result.</source>
        <translation>将某个配置档的反审查设置的各个变体各尝试多次并排名；在你应用结果之前，配置档不会有任何改变。</translation>
    </message>
</context>
<context>
    <name>DialogOtpManager</name>
    <message>
        <source>OTP Manager</source>
        <translation>OTP 管理器</translation>
    </message>
    <message>
        <source>Import</source>
        <translation>导入</translation>
    </message>
    <message>
        <source>Scan QR Code</source>
        <translation>扫描二维码</translation>
    </message>
    <message>
        <source>OTP</source>
        <translation>OTP</translation>
    </message>
    <message>
        <source>No usable code; check the secret.</source>
        <translation>没有可用的代码；检查一下密钥。</translation>
    </message>
    <message>
        <source>Copied</source>
        <translation>已复制</translation>
    </message>
    <message>
        <source>Confirmation</source>
        <translation>确认</translation>
    </message>
    <message>
        <source>Delete &quot;%1&quot;? Its secret cannot be recovered.</source>
        <translation>删除 &quot;%1&quot; 吗? 它的密钥不能被恢复。</translation>
    </message>
    <message>
        <source>Failed to store the OTP profile.</source>
        <translation>存储 OTP 配置档失败。</translation>
    </message>
    <message>
        <source>This one as otpauth:// link and QR</source>
        <translation>这一个为 otpauth:// 链接和 QR 形式</translation>
    </message>
    <message>
        <source>All as otpauth-migration:// link and QR</source>
        <translation>全部为 otpauth-migration:// 链接和 QR 形式</translation>
    </message>
    <message>
        <source>All as JSON file...</source>
        <translation>全部为 JSON 文件...</translation>
    </message>
    <message>
        <source>Add manually...</source>
        <translation>手动添加...</translation>
    </message>
    <message>
        <source>From link or text...</source>
        <translation>来自链接或文本...</translation>
    </message>
    <message>
        <source>Import OTP</source>
        <translation>导入 OTP</translation>
    </message>
    <message>
        <source>Unnamed secret</source>
        <translation>未命名密钥</translation>
    </message>
    <message>
        <source>Not imported:</source>
        <translation>未导入:</translation>
    </message>
    <message numerus="yes">
        <source>…and %n more</source>
        <translation>
            <numerusform>…以及其它 %n 项</numerusform>
        </translation>
    </message>
    <message>
        <source>Paste otpauth:// links, an otpauth-migration:// link, a JSON export, or just a base32 secret.</source>
        <translation>粘贴（多个） otpauth:// 链接，一个 otpauth-migration:// 链接、一个 JSON 导出或就一个 base32 密钥。</translation>
    </message>
    <message>
        <source>From clipboard</source>
        <translation>来自剪贴板</translation>
    </message>
    <message>
        <source>From QR image file...</source>
        <translation>来自 QR 图像文件...</translation>
    </message>
    <message>
        <source>Unable to capture screen</source>
        <translation>无法捕获屏幕</translation>
    </message>
    <message>
        <source>QR Code not found</source>
        <translation>未找到二维码</translation>
    </message>
    <message>
        <source>No QR code found in the clipboard image.</source>
        <translation>在剪贴板图像中找不到 QR 码。</translation>
    </message>
    <message>
        <source>Clipboard is empty.</source>
        <translation>剪贴板是空的。</translation>
    </message>
    <message>
        <source>QR images and exports (*.png *.jpg *.jpeg *.bmp *.gif *.json *.txt);;All files (*)</source>
        <translation>QR 图像和导出内容 (*.png *.jpg *.jpeg *.bmp *.gif *.json *.txt);;所有文件 (*)</translation>
    </message>
    <message>
        <source>%1: cannot be opened</source>
        <translation>%1: 无法打开</translation>
    </message>
    <message>
        <source>%1: larger than 50 MB, skipped</source>
        <translation>%1: 大于 50 MB，已跳过</translation>
    </message>
    <message>
        <source>%1: no QR code found</source>
        <translation>%1: 找不到二维码</translation>
    </message>
    <message>
        <source>Nothing to import.</source>
        <translation>没有导入的内容。</translation>
    </message>
    <message>
        <source>Nothing could be imported.</source>
        <translation>没有可被导入的内容。</translation>
    </message>
    <message>
        <source>%1: already in the list.</source>
        <translation>%1: 已在列表中。</translation>
    </message>
    <message>
        <source>OTP %1</source>
        <translation>OTP %1</translation>
    </message>
    <message>
        <source>%1: could not be stored.</source>
        <translation>%1: 无法被存储。</translation>
    </message>
    <message>
        <source>Imported %1 OTP profile(s).</source>
        <translation>已导入 %1 个 OTP 配置档。</translation>
    </message>
    <message>
        <source>Export OTP</source>
        <translation>导出 OTP</translation>
    </message>
    <message>
        <source>None of the selected profiles could be exported.</source>
        <translation>选定的配置档没有一个可被导出。</translation>
    </message>
    <message>
        <source>JSON (*.json)</source>
        <translation>JSON (*.json)</translation>
    </message>
    <message>
        <source>Cannot write to: %1</source>
        <translation>无法写到: %1</translation>
    </message>
    <message>
        <source>Exported %1 OTP profile(s) to:
%2</source>
        <translation>导出了 %1 个 OTP 配置档到:
%2</translation>
    </message>
</context>
<context>
    <name>DialogPresetSettings</name>
    <message>
        <source>Preset Settings</source>
        <translation>预设设置</translation>
    </message>
    <message>
        <source>Multiplex</source>
        <translation>多路复用</translation>
    </message>
    <message>
        <source>Default On</source>
        <translation>默认开启</translation>
    </message>
    <message>
        <source>Default Mux On</source>
        <translation>默认复用开启</translation>
    </message>
    <message>
        <source>TLS Fragment</source>
        <translation>TLS 分片</translation>
    </message>
    <message>
        <source>Implementation</source>
        <translation>实现</translation>
    </message>
    <message>
        <source>&lt;html&gt;&lt;head/&gt;&lt;body&gt;&lt;p&gt;Which TLS fragmentation implementation profiles use: built-in (sing-box tls.fragment) or custom (Hiddify dialer-level ClientHello fragmentation).&lt;/p&gt;&lt;/body&gt;&lt;/html&gt;</source>
        <translation>&lt;html&gt;&lt;head/&gt;&lt;body&gt;&lt;p&gt;配置档使用的 TLS 分片实现： built-in (内置实现：sing-box tls.fragment) 或 custom (自定义实现：Hiddify 拨号器级别 ClientHello 分段)。&lt;/p&gt;&lt;/body&gt;&lt;/html&gt;</translation>
    </message>
    <message>
        <source>Size</source>
        <translation>大小</translation>
    </message>
    <message>
        <source>&lt;html&gt;&lt;head/&gt;&lt;body&gt;&lt;p&gt;Custom implementation only: bytes per ClientHello fragment, as a range min-max (e.g. 10-100).&lt;/p&gt;&lt;/body&gt;&lt;/html&gt;</source>
        <translation>&lt;html&gt;&lt;head/&gt;&lt;body&gt;&lt;p&gt;仅适用于 Custom (自定义)实现：每个 ClientHello 片段的字节数，采用“最小-最大”取值范围的格式（如 10-100）。&lt;/p&gt;&lt;/body&gt;&lt;/html&gt;</translation>
    </message>
    <message>
        <source>Sleep</source>
        <translation>睡眠</translation>
    </message>
    <message>
        <source>&lt;html&gt;&lt;head/&gt;&lt;body&gt;&lt;p&gt;Custom implementation only: milliseconds to sleep between fragment bursts, as a range min-max (e.g. 2-5).&lt;/p&gt;&lt;/body&gt;&lt;/html&gt;</source>
        <translation>&lt;html&gt;&lt;head/&gt;&lt;body&gt;&lt;p&gt;仅适用于 Custom （自定义）实现：分片突发间的睡眠时间，以毫秒计算，采用“最小-最大”取值范围的格式（如 2-5）。&lt;/p&gt;&lt;/body&gt;&lt;/html&gt;</translation>
    </message>
    <message>
        <source>&lt;html&gt;&lt;head/&gt;&lt;body&gt;&lt;p&gt;Fragment profiles whose Fragment setting is left on &amp;quot;Keep Default&amp;quot;.&lt;/p&gt;&lt;/body&gt;&lt;/html&gt;</source>
        <translation>&lt;html&gt;&lt;head/&gt;&lt;body&gt;&lt;p&gt;分片设置保留为 &amp;quot;Keep Default&amp;quot; (保持默认)的分段配置档。&lt;/p&gt;&lt;/body&gt;&lt;/html&gt;</translation>
    </message>
    <message>
        <source>TLS Tricks</source>
        <translation>TLS 技巧</translation>
    </message>
    <message>
        <source>&lt;html&gt;&lt;head/&gt;&lt;body&gt;&lt;p&gt;Enable mixed-case SNI for profiles whose TLS Tricks setting is left on &amp;quot;Keep Default&amp;quot;.&lt;/p&gt;&lt;/body&gt;&lt;/html&gt;</source>
        <translation>&lt;html&gt;&lt;head/&gt;&lt;body&gt;&lt;p&gt;对其 TLS Tricks （TLS 技巧，一些技术选项）设置保留为 &amp;quot;Keep Default&amp;quot;（保持默认）的配置档启用混合大小写的 SNI。&lt;/p&gt;&lt;/body&gt;&lt;/html&gt;</translation>
    </message>
    <message>
        <source>Default Fingerprint</source>
        <translation>默认指纹</translation>
    </message>
    <message>
        <source>TLS Spoof</source>
        <translation>TLS 伪装</translation>
    </message>
    <message>
        <source>&lt;html&gt;&lt;head/&gt;&lt;body&gt;&lt;p&gt;Inject a forged TLS ClientHello carrying this SNI before the real one, to pass SNI filtering that allows specific hostnames. Profiles that set their own Spoof SNI override this one.&lt;/p&gt;&lt;p&gt;Client only. Requires elevated privileges (root on macOS, CAP_NET_RAW and CAP_NET_ADMIN on Linux, Administrator on Windows for the WinDivert driver). Not supported on Windows ARM64. &amp;quot;wrong-timestamp&amp;quot; is not supported on macOS.&lt;/p&gt;&lt;/body&gt;&lt;/html&gt;</source>
        <translation>&lt;html&gt;&lt;head/&gt;&lt;body&gt;&lt;p&gt;在真实的 TLS ClientHello 前，注入一个承载这个 SNI 的伪造 TLS ClientHello，以传递允许特定主机名的 SNI 过滤。 设置其自有的欺骗 SNI 的配置档会凌驾于这个。&lt;/p&gt;&lt;p&gt;仅客户端。需要提权 (在 macOS 上的 root，在 Linux 上的 CAP_NET_RAW 和 CAP_NET_ADMIN，在 Windows 上的 Administrator 用于 WinDivert 驱动程序)。在 Windows ARM64 上不被支持。 macOS 上不支持 &amp;quot;wrong-timestamp&amp;quot;。&lt;/p&gt;&lt;/body&gt;&lt;/html&gt;</translation>
    </message>
    <message>
        <source>Spoof SNI</source>
        <translation>伪装 SNI</translation>
    </message>
    <message>
        <source>Allowed SNI to forge</source>
        <translation>允许 SNI 伪装</translation>
    </message>
    <message>
        <source>Method</source>
        <translation>方法</translation>
    </message>
    <message>
        <source>&lt;html&gt;&lt;head/&gt;&lt;body&gt;&lt;p&gt;How the forged ClientHello is made invalid for the server, so only the filter acts on it. Empty keeps the core&apos;s default.&lt;/p&gt;&lt;/body&gt;&lt;/html&gt;</source>
        <translation>&lt;html&gt;&lt;head/&gt;&lt;body&gt;&lt;p&gt;如何使伪造的 ClientHello 对服务器无效，从而仅由过滤器对其进行处理。留空则保持核心的默认设置。&lt;/p&gt;&lt;/body&gt;&lt;/html&gt;</translation>
    </message>
    <message>
        <source>&lt;html&gt;&lt;head/&gt;&lt;body&gt;&lt;p&gt;Spoof profiles whose TLS Spoof setting is left on &amp;quot;Keep Default&amp;quot;.&lt;/p&gt;&lt;/body&gt;&lt;/html&gt;</source>
        <translation>&lt;html&gt;&lt;head/&gt;&lt;body&gt;&lt;p&gt;TLS Spoof (TLS 欺骗)设置保留在 &amp;quot;Keep Default&amp;quot;（保持默认）的伪装配置档。&lt;/p&gt;&lt;/body&gt;&lt;/html&gt;</translation>
    </message>
    <message>
        <source>HTTP/2 &amp;&amp; QUIC</source>
        <translation>HTTP/2 和 QUIC</translation>
    </message>
    <message>
        <source>HTTP/2 parameters also apply to Hysteria, Hysteria2 and TUIC. Empty keeps the core&apos;s default.</source>
        <translation>HTTP/2 参数同样适用于 Hysteria、Hysteria2 和 TUIC。留空则保留使用核心的默认值。</translation>
    </message>
    <message>
        <source>HTTP/2 Parameters</source>
        <translation>HTTP/2 参数</translation>
    </message>
    <message>
        <source>Idle Timeout</source>
        <translation>空闲超时</translation>
    </message>
    <message>
        <source>Keep Alive Period</source>
        <translation>保活周期</translation>
    </message>
    <message>
        <source>Stream Receive Window</source>
        <translation>流接收窗口</translation>
    </message>
    <message>
        <source>Connection Receive Window</source>
        <translation>连接接收窗口</translation>
    </message>
    <message>
        <source>Max Concurrent Streams</source>
        <translation>最大并发流数</translation>
    </message>
    <message>
        <source>QUIC Parameters</source>
        <translation>QUIC 参数</translation>
    </message>
    <message>
        <source>Initial Packet Size</source>
        <translation>初始数据包大小</translation>
    </message>
    <message>
        <source>Disable Path MTU Discovery</source>
        <translation>禁用路径 MTU 发现</translation>
    </message>
</context>
<context>
    <name>DialogScanInstance</name>
    <message>
        <source>IP Scanner</source>
        <translation>IP 扫描器</translation>
    </message>
    <message>
        <source>Name</source>
        <translation>名称</translation>
    </message>
    <message>
        <source>Targets</source>
        <translation>目标</translation>
    </message>
    <message>
        <source>Base IP list</source>
        <translation>基础 IP 列表</translation>
    </message>
    <message>
        <source>Manage</source>
        <translation>管理</translation>
    </message>
    <message>
        <source>Use built-in WARP ranges</source>
        <translation>使用内置 WARP 范围</translation>
    </message>
    <message>
        <source>Ports</source>
        <translation>端口</translation>
    </message>
    <message>
        <source>Ports to scan. Targets without one use 443 (80 for plain HTTP).</source>
        <translation>要扫描的端口。无端口的目标使用 443（纯 HTTP 为 80）。</translation>
    </message>
    <message>
        <source>WARP port presets</source>
        <translation>WARP 端口预设</translation>
    </message>
    <message>
        <source>WARP ports</source>
        <translation>WARP 端口</translation>
    </message>
    <message>
        <source>How these combine with the list&apos;s own ports.</source>
        <translation>这些端口如何与列表自带的端口组合。</translation>
    </message>
    <message>
        <source>Shuffle the target order</source>
        <translation>打乱目标顺序</translation>
    </message>
    <message>
        <source>Off: IPv6 ranges in the list are skipped.</source>
        <translation>关闭：列表中的 IPv6 范围会被跳过。</translation>
    </message>
    <message>
        <source>Scan IPv6 addresses</source>
        <translation>扫描 IPv6 地址</translation>
    </message>
    <message>
        <source>Stop after (results)</source>
        <translation>停止结果数</translation>
    </message>
    <message>
        <source>Stop after this many results. Empty: no limit.</source>
        <translation>达到此数量的结果后停止。留空：无限制。</translation>
    </message>
    <message>
        <source>Concurrency</source>
        <translation>并发数</translation>
    </message>
    <message>
        <source>Targets checked at the same time.</source>
        <translation>同时检查的目标数。</translation>
    </message>
    <message>
        <source>Spawn interval (ms)</source>
        <translation>生成间隔 (毫秒)</translation>
    </message>
    <message>
        <source>Delay between starting two probes.</source>
        <translation>启动两个探针间的延时。</translation>
    </message>
    <message>
        <source>Phases</source>
        <translation>阶段</translation>
    </message>
    <message>
        <source>Results</source>
        <translation>结果</translation>
    </message>
    <message>
        <source>Save as IP list</source>
        <translation>保存为 IP 列表</translation>
    </message>
    <message>
        <source>Copy the selected entries, or all if none are selected.</source>
        <translation>复制选中的条目；未选中时复制全部。</translation>
    </message>
    <message>
        <source>Copy</source>
        <translation>复制</translation>
    </message>
    <message>
        <source>Export</source>
        <translation>导出</translation>
    </message>
    <message>
        <source>Remove selected</source>
        <translation>移除选定项</translation>
    </message>
    <message>
        <source>Start</source>
        <translation>启动</translation>
    </message>
    <message>
        <source>Pause</source>
        <translation>暂停</translation>
    </message>
    <message>
        <source>OK</source>
        <translation>确定</translation>
    </message>
    <message>
        <source>Must be from %1 to %2.</source>
        <translation>必须在 %1 到 %2 之间。</translation>
    </message>
    <message>
        <source>Must not contain control characters.</source>
        <translation>不能包含控制字符。</translation>
    </message>
    <message>
        <source>Use % only in an escape such as %20 (write a literal % as %25), for example /generate_204.</source>
        <translation>仅在转义符中使用 %，例如 %20（字面上的 % 写成 %25），例如 /generate_204。</translation>
    </message>
    <message>
        <source>The minimum TLS version is higher than the maximum, so every handshake would fail.</source>
        <translation>最小的 TLS 版本比最大的还高，所以每次握手都会失败。</translation>
    </message>
    <message>
        <source>HTTP/2 needs &quot;h2&quot; in the ALPN list; add it or leave ALPN empty.</source>
        <translation>HTTP/2 需要 &quot;h2&quot; 在 ALPN 列表中；添加它或留空 ALPN 。</translation>
    </message>
    <message>
        <source>The method &quot;None&quot; only checks the TLS handshake, so it needs TLS.</source>
        <translation>方法“None” (无)只检查 TLS 握手，所以它需要 TLS。</translation>
    </message>
    <message>
        <source>HTTP/3 needs TLS.</source>
        <translation>HTTP/3 需要 TLS。</translation>
    </message>
    <message>
        <source>Poll: relays one tunnel query</source>
        <translation>轮询：​转发一条隧道查询</translation>
    </message>
    <message>
        <source>Query names: also relays names as long as the profile sends</source>
        <translation>查询名称：还转发配置档发送的名称</translation>
    </message>
    <message>
        <source>Handshake: also opens a tunnel session</source>
        <translation>握手：还打开一个隧道会话</translation>
    </message>
    <message>
        <source>Quick: relays the smallest packets</source>
        <translation>快速：转发最小的数据包</translation>
    </message>
    <message>
        <source>Full: also measures the upload and download MTU</source>
        <translation>完整：​还测量上传和下载方向的 MTU</translation>
    </message>
    <message>
        <source>Ignore the list&apos;s ports</source>
        <translation>忽略该列表的端口</translation>
    </message>
    <message>
        <source>Merge with the list&apos;s ports</source>
        <translation>与列表的端口合并</translation>
    </message>
    <message>
        <source>Only the list&apos;s ports</source>
        <translation>仅该列表的端口</translation>
    </message>
    <message>
        <source>Entries without a port use 443 (80 for plain HTTP).</source>
        <translation>无端口的条目使用 443（纯 HTTP 为 80）。</translation>
    </message>
    <message>
        <source>No limit</source>
        <translation>无限制</translation>
    </message>
    <message>
        <source>Use Cloudflare&apos;s WARP ranges and ports for this mode.</source>
        <translation>使用此模式对应的 Cloudflare WARP 地址范围和端口。</translation>
    </message>
    <message>
        <source>Each address is tried as a resolver; the default port is 53.</source>
        <translation>每个地址都作为 DNS 解析器尝试；默认端口为 53。</translation>
    </message>
    <message>
        <source>ICMP (ping)</source>
        <translation>ICMP (ping)</translation>
    </message>
    <message>
        <source>Timeout (ms)</source>
        <translation>超时 (毫秒)</translation>
    </message>
    <message>
        <source>Echo requests</source>
        <translation>回显请求</translation>
    </message>
    <message>
        <source>TCP connect</source>
        <translation>TCP 连接</translation>
    </message>
    <message>
        <source>Attempts</source>
        <translation>尝试次数</translation>
    </message>
    <message>
        <source>TLS / HTTP</source>
        <translation>TLS / HTTP</translation>
    </message>
    <message>
        <source>Use TLS</source>
        <translation>使用 TLS</translation>
    </message>
    <message>
        <source>The target address</source>
        <translation>目标地址</translation>
    </message>
    <message>
        <source>SNI</source>
        <translation>SNI</translation>
    </message>
    <message>
        <source>The SNI, or the target address</source>
        <translation>SNI 或目标地址</translation>
    </message>
    <message>
        <source>Host header</source>
        <translation>主机标头</translation>
    </message>
    <message>
        <source>Path</source>
        <translation>路径</translation>
    </message>
    <message>
        <source>None (TLS handshake only)</source>
        <translation>无 (仅 TLS 握手)</translation>
    </message>
    <message>
        <source>A request passes only when the answer is 200 OK.</source>
        <translation>仅当应答为 200 OK 时请求才通过。</translation>
    </message>
    <message>
        <source>Method</source>
        <translation>方法</translation>
    </message>
    <message>
        <source>HTTP version</source>
        <translation>HTTP 版本</translation>
    </message>
    <message>
        <source>From the HTTP version</source>
        <translation>来自 HTTP 版本</translation>
    </message>
    <message>
        <source>ALPN</source>
        <translation>ALPN</translation>
    </message>
    <message>
        <source>Default</source>
        <translation>默认</translation>
    </message>
    <message>
        <source>Minimum TLS version</source>
        <translation>最低 TLS 版本</translation>
    </message>
    <message>
        <source>Maximum TLS version</source>
        <translation>最高 TLS 版本</translation>
    </message>
    <message>
        <source>uTLS fingerprint</source>
        <translation>uTLS 指纹</translation>
    </message>
    <message>
        <source>Allow insecure certificates</source>
        <translation>允许不安全证书</translation>
    </message>
    <message>
        <source>Disable SNI</source>
        <translation>禁用 SNI (不发送服务器名称指示)</translation>
    </message>
    <message>
        <source>Fragment the TLS handshake</source>
        <translation>分片 TLS 握手</translation>
    </message>
    <message>
        <source>Fragment fallback delay (ms)</source>
        <translation>分片回退延时 (毫秒)</translation>
    </message>
    <message>
        <source>Fragment TLS records</source>
        <translation>分片 TLS 记录</translation>
    </message>
    <message>
        <source>Mixed-case SNI</source>
        <translation>混合大小写 SNI</translation>
    </message>
    <message>
        <source>Config test</source>
        <translation>配置测试</translation>
    </message>
    <message>
        <source>Candidates that pass the probes are tested through a copy of this profile pointed at each of them.</source>
        <translation>通过探针的候选者会通过一个指向它们中每一个的这个配置档的副本进行测试。</translation>
    </message>
    <message>
        <source>Profile</source>
        <translation>配置档</translation>
    </message>
    <message>
        <source>Test URL</source>
        <translation>测试 URL</translation>
    </message>
    <message>
        <source>Report warm latency (two requests per target)</source>
        <translation>报告热机延时 (每个目标的两次请求)</translation>
    </message>
    <message>
        <source>WARP test</source>
        <translation>WARP 测试</translation>
    </message>
    <message>
        <source>Candidates that pass the probes are tested by connecting to WARP through them.</source>
        <translation>通过探测的候选项将通过经它们连接到 WARP 进行测试。</translation>
    </message>
    <message>
        <source>Mode</source>
        <translation>模式</translation>
    </message>
    <message>
        <source>HTTP/3, falling back to HTTP/2</source>
        <translation>HTTP/3, 回退到 HTTP/2</translation>
    </message>
    <message>
        <source>HTTP/3 only</source>
        <translation>仅 HTTP/3</translation>
    </message>
    <message>
        <source>HTTP/2 only</source>
        <translation>仅 HTTP/2</translation>
    </message>
    <message>
        <source>MASQUE transport</source>
        <translation>MASQUE 传输</translation>
    </message>
    <message>
        <source>Dedicated for this scan (recommended)</source>
        <translation>专用于这个扫描 (推荐)</translation>
    </message>
    <message>
        <source>From a profile</source>
        <translation>来自配置档</translation>
    </message>
    <message>
        <source>Built-in WARP settings</source>
        <translation>内置 WARP 设置</translation>
    </message>
    <message>
        <source>Identity</source>
        <translation>身份</translation>
    </message>
    <message>
        <source>Register</source>
        <translation>注册</translation>
    </message>
    <message>
        <source>Device</source>
        <translation>设备</translation>
    </message>
    <message>
        <source>MTU</source>
        <translation>MTU</translation>
    </message>
    <message>
        <source>Junk packets (Jc)</source>
        <translation>垃圾数据包 (Jc)</translation>
    </message>
    <message>
        <source>Junk minimum size (Jmin)</source>
        <translation>垃圾数据包最小大小 (Jmin)</translation>
    </message>
    <message>
        <source>Junk maximum size (Jmax)</source>
        <translation>垃圾数据包最大大小 (Jmax)</translation>
    </message>
    <message>
        <source>DNS tunnel test</source>
        <translation>DNS 隧道测试</translation>
    </message>
    <message>
        <source>Each address is tried as the only resolver of this profile; those that relay its tunnel are kept.</source>
        <translation>依次将每个地址作为这个配置档的唯一 DNS 解析器进行尝试；能够转发其隧道流量的地址会被保留。</translation>
    </message>
    <message>
        <source>Test</source>
        <translation>测试</translation>
    </message>
    <message>
        <source>Bounds the whole test of one resolver. Empty: the default of the chosen test.</source>
        <translation>限制单个 DNS 解析器的完整测试的总时长。留空：使用所选测试的默认值。</translation>
    </message>
    <message>
        <source>&quot;%1&quot; is not a valid port (1-65535).</source>
        <translation>&quot;%1&quot; 不是有效的端口 (1-65535)。</translation>
    </message>
    <message>
        <source>Jmin must not be larger than Jmax.</source>
        <translation>Jmin 一定不能大于 Jmax。</translation>
    </message>
    <message>
        <source>Use as resolvers</source>
        <translation>用作解析器</translation>
    </message>
    <message>
        <source>Add this list to the profile&apos;s resolvers.</source>
        <translation>将此列表添加到配置档的 DNS 解析器中。</translation>
    </message>
    <message>
        <source>WARP scan</source>
        <translation>WARP 扫描</translation>
    </message>
    <message>
        <source>DNS resolver scan</source>
        <translation>DNS 解析器扫描</translation>
    </message>
    <message>
        <source>Generic scan</source>
        <translation>通用扫描</translation>
    </message>
    <message>
        <source>%1 - IP Scanner</source>
        <translation>%1 - IP 扫描器</translation>
    </message>
    <message>
        <source>Enter a name for the scan.</source>
        <translation>输入该扫描的名称。</translation>
    </message>
    <message>
        <source>Stop after must be between %1 and %2 results; leave it empty for no limit.</source>
        <translation>停止数量必须介于 %1 到 %2 个结果之间；留空表示无限制。</translation>
    </message>
    <message>
        <source>Concurrency must be between %1 and %2.</source>
        <translation>并发数必须介于 %1 和 %2 之间。</translation>
    </message>
    <message>
        <source>Spawn interval must be between %1 and %2 ms.</source>
        <translation>生成间隔必须在 %1 和 %2 毫秒之间。</translation>
    </message>
    <message>
        <source>ICMP (ping): the timeout must be between %1 and %2 ms.</source>
        <translation>ICMP (ping): 超时时间必须介于 %1 和 %2 毫秒之间。</translation>
    </message>
    <message>
        <source>ICMP (ping): echo requests must be between %1 and %2.</source>
        <translation>ICMP (ping): 回显请求必须在 %1 和 %2 毫秒之间。</translation>
    </message>
    <message>
        <source>DNS tunnel test: the timeout must be between %1 and %2 ms.</source>
        <translation>DNS 隧道测试：超时时间必须介于 %1 和 %2 毫秒之间。</translation>
    </message>
    <message>
        <source>TCP connect: the timeout must be between %1 and %2 ms.</source>
        <translation>TCP 连接: 超时时间必须在 %1 到 %2 毫秒之间。</translation>
    </message>
    <message>
        <source>TCP connect: attempts must be between %1 and %2.</source>
        <translation>TCP 连接：尝试次数必须在 %1 和 %2 之间。</translation>
    </message>
    <message>
        <source>TLS / HTTP: the fragment fallback delay must be between %1 and %2 ms.</source>
        <translation>TLS / HTTP: 分片回退延迟必须在 %1 和 %2 毫秒之间。</translation>
    </message>
    <message>
        <source>TLS / HTTP: the timeout must be between %1 and %2 ms.</source>
        <translation>TLS / HTTP: 超时时间必须在 %1 和 %2 毫秒之间。</translation>
    </message>
    <message>
        <source>Config test: the timeout must be between %1 and %2 ms.</source>
        <translation>配置测试: 超时时间必须在 %1 到 %2 毫秒之间。</translation>
    </message>
    <message>
        <source>WARP test: the MTU must be between %1 and %2.</source>
        <translation>WARP 测试: MTU 必须在 %1 和 %2 之间。</translation>
    </message>
    <message>
        <source>WARP test: Jc must be between %1 and %2.</source>
        <translation>WARP 测试: Jc 必须在 %1 和 %2 之间。</translation>
    </message>
    <message>
        <source>WARP test: Jmin must be between %1 and %2.</source>
        <translation>WARP 测试: Jmin 必须在 %1 和 %2 之间。</translation>
    </message>
    <message>
        <source>WARP test: Jmax must be between %1 and %2.</source>
        <translation>WARP 测试: Jmax 必须在 %1 和 %2 之间。</translation>
    </message>
    <message>
        <source>WARP test: the timeout must be between %1 and %2 ms.</source>
        <translation>WARP 测试: 超时时间必须在 %1 到 %2 毫秒之间。</translation>
    </message>
    <message>
        <source>WARP test: Jmin must not be larger than Jmax.</source>
        <translation>WARP 测试: Jmin 必须不大于 Jmax。</translation>
    </message>
    <message>
        <source>Pause the scan before changing its settings.</source>
        <translation>更改扫描设置之前暂停扫描。</translation>
    </message>
    <message>
        <source>The scan no longer exists.</source>
        <translation>扫描不再存在。</translation>
    </message>
    <message>
        <source>Failed to store the scan.</source>
        <translation>存储该扫描失败。</translation>
    </message>
    <message>
        <source>The settings could not be saved:
%1
Close without saving?</source>
        <translation>设置无法保存:
%1
不保存就关闭吗?</translation>
    </message>
    <message>
        <source>Select an IP list…</source>
        <translation>选择一个 IP 列表...</translation>
    </message>
    <message numerus="yes">
        <source>%1 (%n entries)</source>
        <translation>
            <numerusform>%1 (%n 项)</numerusform>
        </translation>
    </message>
    <message>
        <source>None</source>
        <translation>无</translation>
    </message>
    <message>
        <source>Select a profile…</source>
        <translation>选择一个配置档...</translation>
    </message>
    <message>
        <source>Registering a WARP device…</source>
        <translation>正在注册 WARP 设备...</translation>
    </message>
    <message>
        <source>No device is registered for this scan yet.</source>
        <translation>尚未为这个扫描注册设备。</translation>
    </message>
    <message>
        <source>The registered device is for %1; register a new one for this mode.</source>
        <translation>注册的设备是用于 %1 的; 请为这个模式注册一个新设备。</translation>
    </message>
    <message>
        <source>A WARP device is registered for this scan.</source>
        <translation>为这个扫描注册一个 WARP 设备。</translation>
    </message>
    <message>
        <source>Register again</source>
        <translation>再次注册</translation>
    </message>
    <message>
        <source>Failed to store the IP list.</source>
        <translation>存储 IP 列表失败。</translation>
    </message>
    <message>
        <source>MASQUE: %1</source>
        <translation>MASQUE: %1</translation>
    </message>
    <message>
        <source>WireGuard: %1</source>
        <translation>WireGuard: %1</translation>
    </message>
    <message numerus="yes">
        <source>WireGuard, all %n known ports</source>
        <translation>
            <numerusform>WireGuard, 所有 %n 个已知端口</numerusform>
        </translation>
    </message>
    <message>
        <source>WARP registration failed</source>
        <translation>WARP 注册失败</translation>
    </message>
    <message>
        <source>Unknown error</source>
        <translation>未知错误</translation>
    </message>
    <message>
        <source>Resume</source>
        <translation>继续</translation>
    </message>
    <message>
        <source>No results yet.</source>
        <translation>还没有结果。</translation>
    </message>
    <message>
        <source>Showing the first %1 of %2 results</source>
        <translation>显示 %2 个结果的前 %1 个</translation>
    </message>
    <message numerus="yes">
        <source>%n result(s)</source>
        <translation>
            <numerusform>%n 个结果</numerusform>
        </translation>
    </message>
    <message>
        <source>Name of the new IP list:</source>
        <translation>新 IP 列表的名称:</translation>
    </message>
    <message>
        <source>%1 results</source>
        <translation>%1 个结果</translation>
    </message>
    <message numerus="yes">
        <source>Saved %n entries to the IP list &quot;%1&quot;.</source>
        <translation>
            <numerusform>已保存 %n 项到 IP 列表 &quot;%1&quot;。</numerusform>
        </translation>
    </message>
    <message>
        <source>The scan&apos;s profile was deleted or is no longer a DNS tunnel profile.</source>
        <translation>扫描所用的配置档已被删除，或不再是 DNS 隧道配置档。</translation>
    </message>
    <message>
        <source>&quot;%1&quot; is open in an editor; close it and try again.</source>
        <translation>“%1” 在编辑器中打开；请将其关闭并再次尝试。</translation>
    </message>
    <message>
        <source>The open editor of &quot;%1&quot; now takes resolvers from the IP list &quot;%2&quot;; press OK there to save it.</source>
        <translation>“%1” 的打开编辑器现在从 IP 列表 “%2” 中获取 DNS 解析器；请按【确定】以保存。</translation>
    </message>
    <message>
        <source>Failed to store the profile.</source>
        <translation>存储配置档失败。</translation>
    </message>
    <message>
        <source>&quot;%1&quot; now takes resolvers from the IP list &quot;%2&quot;.</source>
        <translation>&quot;%1&quot; 现在从 IP 列表 &quot;%2&quot; 中获取 DNS 解析器。</translation>
    </message>
    <message numerus="yes">
        <source>Copied %n entries</source>
        <translation>
            <numerusform>已复制 %n 项</numerusform>
        </translation>
    </message>
    <message>
        <source>Export results</source>
        <translation>导出结果</translation>
    </message>
    <message>
        <source>Text files (*.txt);;All files (*)</source>
        <translation>文本文件 (*.txt);;所有文件 (*)</translation>
    </message>
    <message>
        <source>Cannot write to: %1</source>
        <translation>无法写到: %1</translation>
    </message>
    <message>
        <source>Scan from the beginning</source>
        <translation>从头开始扫描</translation>
    </message>
    <message>
        <source>Rescan current results</source>
        <translation>重新扫描当前结果</translation>
    </message>
    <message>
        <source>QUIC opener</source>
        <translation>QUIC 前导包</translation>
    </message>
    <message>
        <source>Sends a decoy packet before each UDP flow to get past QUIC blocking.</source>
        <translation>在每个 UDP 流之前发送一个诱饵包，以绕过 QUIC 封锁。</translation>
    </message>
    <message>
        <source>&quot;%1&quot; already uses these settings.</source>
        <translation>“%1”已在使用这些设置。</translation>
    </message>
    <message>
        <source>&quot;%1&quot; cannot be searched. %2.</source>
        <translation>无法搜索“%1”。%2。</translation>
    </message>
    <message>
        <source>&quot;%1&quot; could not be copied.</source>
        <translation>无法复制“%1”。</translation>
    </message>
    <message>
        <source>&quot;%1&quot; is open in an editor; close the profile&apos;s editor first, then apply again.</source>
        <translation>“%1”正在编辑器中打开；请先关闭该配置档的编辑器，然后再次应用。</translation>
    </message>
    <message>
        <source>&quot;%1&quot; now uses: %2.</source>
        <translation>“%1”现在使用：%2。</translation>
    </message>
    <message>
        <source>%1 (tuned)</source>
        <translation>%1（已调优）</translation>
    </message>
    <message>
        <source>%1 ms</source>
        <translation>%1 毫秒</translation>
    </message>
    <message>
        <source>%1/%2</source>
        <translation>%1/%2</translation>
    </message>
    <message>
        <source>A censor that resets only some handshakes needs several tries to tell a working variant from a lucky one.</source>
        <translation>若审查只重置部分握手，就需要多次尝试才能区分真正可用的变体和侥幸成功的变体。</translation>
    </message>
    <message>
        <source>Add a copy of the profile with the selected variant.</source>
        <translation>添加带有所选变体的配置档副本。</translation>
    </message>
    <message>
        <source>Apply to profile</source>
        <translation>应用到配置档</translation>
    </message>
    <message>
        <source>At most this many attempts start in any minute.</source>
        <translation>任意一分钟内最多开始这么多次尝试。</translation>
    </message>
    <message>
        <source>Attempt limit</source>
        <translation>尝试次数上限</translation>
    </message>
    <message>
        <source>Attempts per minute</source>
        <translation>每分钟尝试次数</translation>
    </message>
    <message>
        <source>Attempts per variant</source>
        <translation>每个变体的尝试次数</translation>
    </message>
    <message>
        <source>Best: %1 (passed %2/%3, median %4)</source>
        <translation>最佳：%1（通过 %2/%3，中位数 %4）</translation>
    </message>
    <message>
        <source>Choose the profile whose settings to search.</source>
        <translation>选择要搜索其设置的配置档。</translation>
    </message>
    <message>
        <source>Comma-separated, e.g. 100ms, 500ms, 1s.</source>
        <translation>以逗号分隔，例如 100ms, 500ms, 1s。</translation>
    </message>
    <message>
        <source>Comma-separated uTLS fingerprints; &quot;off&quot; tries Go&apos;s own TLS.</source>
        <translation>以逗号分隔的 uTLS 指纹；“off”表示尝试 Go 自带的 TLS。</translation>
    </message>
    <message>
        <source>Confirmed</source>
        <translation>已确认</translation>
    </message>
    <message numerus="yes">
        <source>Copied %n variant(s)</source>
        <translation>
            <numerusform>已复制 %n 个变体</numerusform>
        </translation>
    </message>
    <message>
        <source>Copies the table as tab-separated text.</source>
        <translation>将表格复制为以制表符分隔的文本。</translation>
    </message>
    <message>
        <source>Download URL</source>
        <translation>下载 URL</translation>
    </message>
    <message>
        <source>Download size (bytes)</source>
        <translation>下载大小（字节）</translation>
    </message>
    <message>
        <source>Downloaded to check each MTU. Empty: Cloudflare&apos;s speed test.</source>
        <translation>下载以检查每个 MTU。留空：使用 Cloudflare 的测速。</translation>
    </message>
    <message>
        <source>Dropped early</source>
        <translation>提前淘汰</translation>
    </message>
    <message>
        <source>Each setting is first tried on its own, a switch flipped or a list value by value; the changes that help are then combined.</source>
        <translation>每项设置先单独尝试：开关翻转一次，列表则逐个值尝试；随后将有帮助的更改组合起来。</translation>
    </message>
    <message>
        <source>Every attempt requests this URL through the profile. While the MTU is searched, attempts download from the MTU download URL instead.</source>
        <translation>每次尝试都通过该配置档请求此 URL。搜索 MTU 时，尝试改为从 MTU 下载 URL 下载。</translation>
    </message>
    <message>
        <source>Fallback delays to try</source>
        <translation>要尝试的回退延时</translation>
    </message>
    <message>
        <source>Find working settings</source>
        <translation>查找可用设置</translation>
    </message>
    <message>
        <source>Finds the largest MTU that still works.</source>
        <translation>查找仍可用的最大 MTU。</translation>
    </message>
    <message>
        <source>Fingerprints to try</source>
        <translation>要尝试的指纹</translation>
    </message>
    <message>
        <source>How many bytes each MTU check downloads.</source>
        <translation>每次 MTU 检查下载的字节数。</translation>
    </message>
    <message>
        <source>Largest MTU</source>
        <translation>最大 MTU</translation>
    </message>
    <message>
        <source>Largest working MTU</source>
        <translation>最大可用 MTU</translation>
    </message>
    <message>
        <source>MTU (WireGuard)</source>
        <translation>MTU (WireGuard)</translation>
    </message>
    <message>
        <source>MTU probe</source>
        <translation>MTU 探测</translation>
    </message>
    <message>
        <source>MTU step</source>
        <translation>MTU 步长</translation>
    </message>
    <message>
        <source>MTU: the download size must be between %1 and %2 bytes.</source>
        <translation>MTU：下载大小必须在 %1 到 %2 字节之间。</translation>
    </message>
    <message>
        <source>MTU: the largest MTU must be between %1 and %2; leave it empty for the profile&apos;s own.</source>
        <translation>MTU：最大 MTU 必须在 %1 到 %2 之间；留空则使用配置档自身的值。</translation>
    </message>
    <message>
        <source>MTU: the smallest MTU must be between %1 and %2.</source>
        <translation>MTU：最小 MTU 必须在 %1 到 %2 之间。</translation>
    </message>
    <message>
        <source>MTU: the step must be between %1 and %2.</source>
        <translation>MTU：步长必须在 %1 到 %2 之间。</translation>
    </message>
    <message>
        <source>Median latency</source>
        <translation>延迟中位数</translation>
    </message>
    <message>
        <source>Needs SNI turned on with a domain name, and no REALITY.</source>
        <translation>需要开启 SNI 且使用域名，并且不使用 REALITY。</translation>
    </message>
    <message>
        <source>Needs QUIC, MASQUE over HTTP/3 or WireGuard.</source>
        <translation>需要 QUIC、基于 HTTP/3 的 MASQUE 或 WireGuard。</translation>
    </message>
    <message>
        <source>No variant has passed yet.</source>
        <translation>尚无变体通过。</translation>
    </message>
    <message>
        <source>No variant passed.</source>
        <translation>没有变体通过。</translation>
    </message>
    <message>
        <source>Not usable</source>
        <translation>不可用</translation>
    </message>
    <message>
        <source>Not usable: %1</source>
        <translation>不可用：%1</translation>
    </message>
    <message>
        <source>Note</source>
        <translation>备注</translation>
    </message>
    <message>
        <source>Only WireGuard and AmneziaWG profiles have an MTU to search.</source>
        <translation>只有 WireGuard 和 AmneziaWG 配置档才有可搜索的 MTU。</translation>
    </message>
    <message>
        <source>Only the built-in fragment implementation uses a fallback delay.</source>
        <translation>只有内置的分片实现会使用回退延时。</translation>
    </message>
    <message>
        <source>Pacing</source>
        <translation>节奏</translation>
    </message>
    <message>
        <source>Pacing: attempts per minute must be between %1 and %2.</source>
        <translation>节奏：每分钟尝试次数必须在 %1 到 %2 之间。</translation>
    </message>
    <message>
        <source>Pacing: attempts per variant must be between %1 and %2.</source>
        <translation>节奏：每个变体的尝试次数必须在 %1 到 %2 之间。</translation>
    </message>
    <message>
        <source>Pacing: the attempt limit must be between %1 and %2.</source>
        <translation>节奏：尝试次数上限必须在 %1 到 %2 之间。</translation>
    </message>
    <message>
        <source>Pacing: the time limit must be between %1 and %2 minutes.</source>
        <translation>节奏：时间上限必须在 %1 到 %2 分钟之间。</translation>
    </message>
    <message>
        <source>Pacing: the timeout must be between %1 and %2 ms.</source>
        <translation>节奏：超时时间必须在 %1 到 %2 毫秒之间。</translation>
    </message>
    <message>
        <source>Passed</source>
        <translation>通过</translation>
    </message>
    <message>
        <source>Profile as it is</source>
        <translation>原样的配置档</translation>
    </message>
    <message>
        <source>Recommended</source>
        <translation>推荐</translation>
    </message>
    <message>
        <source>Save as new profile</source>
        <translation>另存为新配置档</translation>
    </message>
    <message>
        <source>Saved &quot;%1&quot; with: %2.</source>
        <translation>已保存“%1”，使用：%2。</translation>
    </message>
    <message>
        <source>Settings</source>
        <translation>设置</translation>
    </message>
    <message>
        <source>Smallest MTU</source>
        <translation>最小 MTU</translation>
    </message>
    <message>
        <source>TLS fragment</source>
        <translation>TLS 分片</translation>
    </message>
    <message>
        <source>TLS record fragment</source>
        <translation>TLS 记录分片</translation>
    </message>
    <message>
        <source>Tested through %1</source>
        <translation>已通过 %1 测试</translation>
    </message>
    <message>
        <source>Tests through %1</source>
        <translation>通过 %1 测试</translation>
    </message>
    <message>
        <source>The largest MTU must be above the smallest; leave it empty for the profile&apos;s own.</source>
        <translation>最大 MTU 必须大于最小 MTU；留空则使用配置档自身的值。</translation>
    </message>
    <message>
        <source>The profile has no server address to test.</source>
        <translation>该配置档没有可测试的服务器地址。</translation>
    </message>
    <message>
        <source>The profile of this search no longer exists.</source>
        <translation>此搜索的配置档已不存在。</translation>
    </message>
    <message>
        <source>The profile this search was made for no longer exists.</source>
        <translation>此搜索所针对的配置档已不存在。</translation>
    </message>
    <message>
        <source>The profile&apos;s own</source>
        <translation>配置档自身的值</translation>
    </message>
    <message>
        <source>The search stops after this many attempts in all.</source>
        <translation>总尝试次数达到此值后搜索停止。</translation>
    </message>
    <message>
        <source>The search stops after this many minutes.</source>
        <translation>搜索在这么多分钟后停止。</translation>
    </message>
    <message>
        <source>The settings could not be applied to &quot;%1&quot;: %2.</source>
        <translation>无法将设置应用到“%1”：%2。</translation>
    </message>
    <message>
        <source>The settings could not be applied to the copy: %1.</source>
        <translation>无法将设置应用到副本：%1。</translation>
    </message>
    <message>
        <source>These results are for another profile; start the search to test the one chosen now.</source>
        <translation>这些结果属于另一个配置档；请开始搜索以测试当前选择的配置档。</translation>
    </message>
    <message>
        <source>This profile does not use TLS over TCP.</source>
        <translation>此配置档未使用基于 TCP 的 TLS。</translation>
    </message>
    <message>
        <source>This type of profile has no uTLS fingerprint.</source>
        <translation>此类型的配置档没有 uTLS 指纹。</translation>
    </message>
    <message>
        <source>Time limit (minutes)</source>
        <translation>时间上限（分钟）</translation>
    </message>
    <message>
        <source>Variants of this profile&apos;s anti-censorship settings are tested through the core, one attempt at a time. The profile itself changes only when you apply a result.</source>
        <translation>此配置档的反审查设置的各个变体将通过核心逐次测试。只有在你应用结果时，配置档本身才会改变。</translation>
    </message>
    <message>
        <source>What to try</source>
        <translation>要尝试的项目</translation>
    </message>
    <message>
        <source>Save the selected variant into the profile.</source>
        <translation>将所选变体保存到配置档。</translation>
    </message>
</context>
<context>
    <name>DialogScanner</name>
    <message>
        <source>IP Scanner</source>
        <translation>IP 扫描器</translation>
    </message>
    <message>
        <source>No scans yet. Create one with &quot;New scan&quot;.</source>
        <translation>尚未扫描。用“新建扫描”创建一个。</translation>
    </message>
    <message>
        <source>New scan</source>
        <translation>新建扫描</translation>
    </message>
    <message>
        <source>IP Lists</source>
        <translation>IP 列表</translation>
    </message>
    <message>
        <source>Pause the scan first.</source>
        <translation>首先暂停扫描。</translation>
    </message>
    <message>
        <source>Confirmation</source>
        <translation>确认</translation>
    </message>
    <message>
        <source>Delete the scan &quot;%1&quot;?</source>
        <translation>删除扫描 &quot;%1&quot; 吗?</translation>
    </message>
    <message>
        <source>Also delete its result list</source>
        <translation>还删除其结果列表</translation>
    </message>
</context>
<context>
    <name>DialogTrafficStats</name>
    <message>
        <source>Traffic Statistics</source>
        <translation>流量统计</translation>
    </message>
    <message>
        <source>Period:</source>
        <translation>周期:</translation>
    </message>
    <message>
        <source>Last 24 hours</source>
        <translation>最后 24 小时</translation>
    </message>
    <message>
        <source>Last 7 days</source>
        <translation>最后 7 天</translation>
    </message>
    <message>
        <source>Last 30 days</source>
        <translation>最后 30 天</translation>
    </message>
    <message>
        <source>Last 90 days</source>
        <translation>最后 90 天</translation>
    </message>
    <message>
        <source>Refresh</source>
        <translation>刷新</translation>
    </message>
    <message>
        <source>By Profile</source>
        <translation>按配置档</translation>
    </message>
    <message>
        <source>Profile</source>
        <translation>配置档</translation>
    </message>
    <message>
        <source>Group</source>
        <translation>分组</translation>
    </message>
    <message>
        <source>Download</source>
        <translation>下载(↓)</translation>
    </message>
    <message>
        <source>Upload</source>
        <translation>上传(↑)</translation>
    </message>
    <message>
        <source>Total</source>
        <translation>合计(Σ)</translation>
    </message>
    <message>
        <source>By App</source>
        <translation>按应用</translation>
    </message>
    <message>
        <source>App</source>
        <translation>应用</translation>
    </message>
    <message>
        <source>Download: %1     Upload: %2     Total: %3</source>
        <translation>下载(↓): %1  上传(↑): %2  合计(Σ): %3</translation>
    </message>
    <message>
        <source>Direct</source>
        <translation>直连</translation>
    </message>
    <message>
        <source>Profile #%1 (deleted)</source>
        <translation>配置档 №%1 (已删除)</translation>
    </message>
    <message>
        <source>Other</source>
        <translation>其它</translation>
    </message>
    <message>
        <source>Unknown</source>
        <translation>未知</translation>
    </message>
</context>
<context>
    <name>DialogUpdate</name>
    <message>
        <source>Update (Pre-release)</source>
        <translation>更新 (预发布)</translation>
    </message>
    <message>
        <source>Update</source>
        <translation>更新</translation>
    </message>
    <message>
        <source>Throne %1 is available, you are using %2.</source>
        <translation>Throne %1 可供使用，你正在使用 %2。</translation>
    </message>
    <message>
        <source>Release notes are unavailable.</source>
        <translation>暂无发布说明。</translation>
    </message>
    <message>
        <source>Update</source>
        <comment>button</comment>
        <translation>更新</translation>
    </message>
    <message>
        <source>Open in browser</source>
        <translation>浏览器中打开</translation>
    </message>
    <message>
        <source>Close</source>
        <translation>关闭</translation>
    </message>
</context>
<context>
    <name>DialogVPNSettings</name>
    <message>
        <source>Tun Settings</source>
        <translation>Tun 设置</translation>
    </message>
    <message>
        <source>Tun Enable IPv6</source>
        <translation>Tun 启用 IPv6</translation>
    </message>
    <message>
        <source>Strict Route</source>
        <translation>严格路由</translation>
    </message>
    <message>
        <source>&lt;html&gt;&lt;head/&gt;&lt;body&gt;&lt;p&gt;When enabled, Direct IP CIDRs and rule sets will also be added to the Tun setting to be excluded at OS routing level. IMPORTANT: Large rule sets have been reported to cause massive CPU usage in windows, please enable this option with caution.&lt;/p&gt;&lt;/body&gt;&lt;/html&gt;</source>
        <translation>&lt;html&gt;&lt;head/&gt;&lt;body&gt;&lt;p&gt;启用时，Direct IP CIDR 和规则集也将添加到 Tun 设置中，以便在操作系统路由级别被排除。重要提示：已有报告大型规则集会导致 Windows 中的 CPU 占用率大幅上升，请谨慎启用此选项。&lt;/p&gt;&lt;/body&gt;&lt;/html&gt;</translation>
    </message>
    <message>
        <source>Enable Tun Routing</source>
        <translation>启用 Tun 路由</translation>
    </message>
    <message>
        <source>&lt;html&gt;&lt;head/&gt;&lt;body&gt;&lt;p&gt;Improves Tun routing and performance using nftables. While enabled, this machine cannot be used as a network gateway for other devices.&lt;/p&gt;&lt;/body&gt;&lt;/html&gt;</source>
        <translation>&lt;html&gt;&lt;head/&gt;&lt;body&gt;&lt;p&gt;使用 nftables 改善 Tun 路由和性能。在启用时，这台机器无法用作其它设备的网络网关。&lt;/p&gt;&lt;/body&gt;&lt;/html&gt;</translation>
    </message>
    <message>
        <source>Auto Redirect</source>
        <translation>自动重定向</translation>
    </message>
    <message>
        <source>&lt;html&gt;&lt;head/&gt;&lt;body&gt;&lt;p&gt;Forwards bypassed traffic straight out of the network interface instead of rebuilding it as a new connection, which is faster and keeps NAT intact for games and P2P. Applies to UDP and ICMP only; TCP still needs to be sniffed and keeps taking the normal direct path.&lt;/p&gt;&lt;/body&gt;&lt;/html&gt;</source>
        <translation>&lt;html&gt;&lt;head/&gt;&lt;body&gt;&lt;p&gt;转发绕过的流量直接从网络接口发送出去而不是把它重建为一个新连接，这样更快，同时对游戏和 P2P 保持 NAT 不变。仅适用于 UDP 和 ICMP； TCP 仍需要被嗅探并继续采用正常的直连路径。&lt;/p&gt;&lt;/body&gt;&lt;/html&gt;</translation>
    </message>
    <message>
        <source>L3 Bridge Bypass</source>
        <translation>L3 桥接绕过</translation>
    </message>
    <message>
        <source>Tun Address</source>
        <translation>Tun 地址</translation>
    </message>
    <message>
        <source>IPv4 CIDR</source>
        <translation>IPv4 CIDR</translation>
    </message>
    <message>
        <source>IPv6 CIDR</source>
        <translation>IPv6 CIDR</translation>
    </message>
    <message>
        <source>Restore default addresses</source>
        <translation>恢复默认地址</translation>
    </message>
    <message>
        <source>&lt;html&gt;&lt;head/&gt;&lt;body&gt;&lt;p&gt;Address ranges handed straight to the physical NIC instead of being routed through the core, one CIDR per line. Uncheck to let the core hijack private range traffic instead. Loopback and broadcast are always bypassed and cannot be listed here.&lt;/p&gt;&lt;/body&gt;&lt;/html&gt;</source>
        <translation>&lt;html&gt;&lt;head/&gt;&lt;body&gt;&lt;p&gt;地址范围直接交给物理网卡而不是通过核心进行路由，每行一个 CIDR。 取消勾选表示改为让核心劫持私有地址范围的流量。 Loopback (回环)和广播地址总是被绕过，不能在这里列出。&lt;/p&gt;&lt;/body&gt;&lt;/html&gt;</translation>
    </message>
    <message>
        <source>Private Range Bypass</source>
        <translation>私有地址范围绕过</translation>
    </message>
    <message>
        <source>Restore default ranges</source>
        <translation>恢复默认范围</translation>
    </message>
    <message>
        <source>Stops the running profile and restarts the core. Try this when Tun fails to start, then start the profile again.</source>
        <translation>停止正在运行的配置档并重启该核心。当 Tun 启动失败时尝试这个操作，然后再次启动该配置档。</translation>
    </message>
    <message>
        <source>Restart Core</source>
        <translation>重启核心</translation>
    </message>
    <message>
        <source>IPv6 needs an MTU of at least %1; below that the system may drop IPv6 from the Tun.</source>
        <translation>IPv6 需要至少 %1 的 MTU；低于该值，系统可能会从 Tun 中丢弃 IPv6。</translation>
    </message>
    <message>
        <source>This covers every address, which would stop Tun from routing anything.</source>
        <translation>这涵盖了每个地址，这会阻止 Tun 路由任何内容。</translation>
    </message>
</context>
<context>
    <name>DialogVpnAuth</name>
    <message>
        <source>VPN Authentication</source>
        <translation>VPN 认证</translation>
    </message>
    <message>
        <source>Endpoint: %1</source>
        <translation>端点: %1</translation>
    </message>
    <message>
        <source>Reconnect</source>
        <translation>重连</translation>
    </message>
    <message>
        <source>Submit</source>
        <translation>提交</translation>
    </message>
    <message>
        <source>The VPN server sent a message.</source>
        <translation>VPN 服务器发送了一条消息。</translation>
    </message>
    <message>
        <source>Finish signing in at the address shown below, then close this window. The connection continues on its own once the server accepts it.</source>
        <translation>在下方显示的地址处完成登录，然后关闭这个窗口。一旦服务器接受连接则连接自动继续。</translation>
    </message>
    <message>
        <source>Open in Browser</source>
        <translation>在浏览器中打开</translation>
    </message>
    <message>
        <source>This server requires single sign-on in a browser, which Throne does not support yet. Cancel here and use a profile with direct credentials, or supply an authentication cookie in the profile&apos;s advanced settings.</source>
        <translation>这个服务器想要在浏览器中进行单点登录，而 Throne 还未支持。可以在这里取消，然后使用带直接凭证的配置档，或者在配置档的高级设置中提高认证的 cookie。</translation>
    </message>
    <message>
        <source>Unsupported authentication request: %1</source>
        <translation>不支持的验证请求: %1</translation>
    </message>
    <message>
        <source>Expires in %1</source>
        <translation>%1 后过期</translation>
    </message>
    <message>
        <source>Username</source>
        <translation>用户名</translation>
    </message>
    <message>
        <source>Password</source>
        <translation>密码</translation>
    </message>
    <message>
        <source>Answer</source>
        <translation>应答</translation>
    </message>
    <message>
        <source>This request expired.</source>
        <translation>这个请求已过期。</translation>
    </message>
    <message>
        <source>Submitting...</source>
        <translation>正在提交...</translation>
    </message>
</context>
<context>
    <name>EditAdvanced</name>
    <message>
        <source>Dialog</source>
        <translation>对话框</translation>
    </message>
    <message>
        <source>Server Address</source>
        <translation>服务器地址</translation>
    </message>
    <message>
        <source>Dial Fields</source>
        <translation>拨号字段</translation>
    </message>
    <message>
        <source>Reuse Address</source>
        <translation>重用地址</translation>
    </message>
    <message>
        <source>TCP Fast Open</source>
        <translation>TCP 快速开启</translation>
    </message>
    <message>
        <source>UDP Fragment</source>
        <translation>UDP 分片</translation>
    </message>
    <message>
        <source>TCP MultiPath</source>
        <translation>TCP 多路径</translation>
    </message>
    <message>
        <source>Connect Timeout</source>
        <translation>连接超时</translation>
    </message>
    <message>
        <source>Bind Interface</source>
        <translation>绑定接口</translation>
    </message>
    <message>
        <source>IPv4 Bind Address</source>
        <translation>IPv4 绑定地址</translation>
    </message>
    <message>
        <source>IPv6 Bind Address</source>
        <translation>IPv6 绑定地址</translation>
    </message>
    <message>
        <source>Interface</source>
        <translation>接口</translation>
    </message>
    <message>
        <source>&lt;html&gt;&lt;head/&gt;&lt;body&gt;&lt;p&gt;Requires privilege and cannot conflict with existing system interfaces. The internal network stack is used when disabled.&lt;/p&gt;&lt;/body&gt;&lt;/html&gt;</source>
        <translation>&lt;html&gt;&lt;head/&gt;&lt;body&gt;&lt;p&gt;需要特权，且不能与现有的系统接口冲突。 禁用时使用内部网络协议栈。&lt;/p&gt;&lt;/body&gt;&lt;/html&gt;</translation>
    </message>
    <message>
        <source>Use System Interface</source>
        <translation>使用系统接口</translation>
    </message>
    <message>
        <source>&lt;html&gt;&lt;head/&gt;&lt;body&gt;&lt;p&gt;Custom system interface name. A generated name is used by default.&lt;/p&gt;&lt;/body&gt;&lt;/html&gt;</source>
        <translation>&lt;html&gt;&lt;head/&gt;&lt;body&gt;&lt;p&gt;自定义系统接口名称。默认使用生成的名称。&lt;/p&gt;&lt;/body&gt;&lt;/html&gt;</translation>
    </message>
    <message>
        <source>Interface Name</source>
        <translation>接口名称</translation>
    </message>
    <message>
        <source>&lt;html&gt;&lt;head/&gt;&lt;body&gt;&lt;p&gt;UDP NAT expiration time.&lt;/p&gt;&lt;/body&gt;&lt;/html&gt;</source>
        <translation>&lt;html&gt;&lt;head/&gt;&lt;body&gt;&lt;p&gt;UDP NAT 过期时间。&lt;/p&gt;&lt;/body&gt;&lt;/html&gt;</translation>
    </message>
    <message>
        <source>UDP Timeout</source>
        <translation>UDP 超时</translation>
    </message>
    <message>
        <source>5m (default)</source>
        <translation>5m (5分，默认)</translation>
    </message>
    <message>
        <source>&lt;html&gt;&lt;head/&gt;&lt;body&gt;&lt;p&gt;UDP NAT mapping behavior. endpoint_independent is used by default.&lt;/p&gt;&lt;/body&gt;&lt;/html&gt;</source>
        <translation>&lt;html&gt;&lt;head/&gt;&lt;body&gt;&lt;p&gt;UDP NAT 映射行为。默认使用 endpoint_independent (端点独立)。&lt;/p&gt;&lt;/body&gt;&lt;/html&gt;</translation>
    </message>
    <message>
        <source>UDP Mapping</source>
        <translation>UDP 映射</translation>
    </message>
    <message>
        <source>&lt;html&gt;&lt;head/&gt;&lt;body&gt;&lt;p&gt;UDP NAT filtering behavior. endpoint_independent is used by default.&lt;/p&gt;&lt;/body&gt;&lt;/html&gt;</source>
        <translation>&lt;html&gt;&lt;head/&gt;&lt;body&gt;&lt;p&gt;UDP NAT 过滤行为。默认使用 endpoint_independent (端点独立)。&lt;/p&gt;&lt;/body&gt;&lt;/html&gt;</translation>
    </message>
    <message>
        <source>UDP Filtering</source>
        <translation>UDP 过滤</translation>
    </message>
    <message>
        <source>&lt;html&gt;&lt;head/&gt;&lt;body&gt;&lt;p&gt;Maximum number of UDP NAT entries.&lt;/p&gt;&lt;/body&gt;&lt;/html&gt;</source>
        <translation>&lt;html&gt;&lt;head/&gt;&lt;body&gt;&lt;p&gt;UDP NAT 条目的最大数量。&lt;/p&gt;&lt;/body&gt;&lt;/html&gt;</translation>
    </message>
    <message>
        <source>UDP NAT Max</source>
        <translation>UDP NAT 上限</translation>
    </message>
    <message>
        <source>TLS</source>
        <translation>TLS</translation>
    </message>
    <message>
        <source>Disable SNI</source>
        <translation>禁用 SNI (不发送服务器名称指示)</translation>
    </message>
    <message>
        <source>Enable ECH</source>
        <translation>启用 ECH</translation>
    </message>
    <message>
        <source>TLS Min Version</source>
        <translation>TLS 最低版本</translation>
    </message>
    <message>
        <source>TLS Max Version</source>
        <translation>TLS 最高版本</translation>
    </message>
    <message>
        <source>&lt;html&gt;&lt;head/&gt;&lt;body&gt;&lt;p&gt;Whether this profile injects a forged ClientHello. &amp;quot;Keep Default&amp;quot; follows Preset Settings, unless a Spoof SNI is set below.&lt;/p&gt;&lt;/body&gt;&lt;/html&gt;</source>
        <translation>&lt;html&gt;&lt;head/&gt;&lt;body&gt;&lt;p&gt;这个配置档是否注入一个伪造的 ClientHello。 &amp;quot;Keep Default&amp;quot; (保持默认)表示使用“预设设置”，除非下面设定了“伪装 SNI”。&lt;/p&gt;&lt;/body&gt;&lt;/html&gt;</translation>
    </message>
    <message>
        <source>TLS Spoof</source>
        <translation>TLS 伪装</translation>
    </message>
    <message>
        <source>Keep Default</source>
        <translation>保持默认</translation>
    </message>
    <message>
        <source>On</source>
        <translation>开启</translation>
    </message>
    <message>
        <source>Off</source>
        <translation>关闭</translation>
    </message>
    <message>
        <source>&lt;html&gt;&lt;head/&gt;&lt;body&gt;&lt;p&gt;Inject a forged TLS ClientHello carrying this SNI before the real one, to pass SNI filtering that allows specific hostnames.&lt;/p&gt;&lt;p&gt;Client only. Requires elevated privileges (root / Administrator). Not supported on Windows ARM64. &amp;quot;wrong-timestamp&amp;quot; is not supported on macOS.&lt;/p&gt;&lt;p&gt;Leave empty to inherit the global value from Preset Settings.&lt;/p&gt;&lt;/body&gt;&lt;/html&gt;</source>
        <translation>&lt;html&gt;&lt;head/&gt;&lt;body&gt;&lt;p&gt;在真实的 TLS ClientHello 之前注入一个携带这个 SNI 的伪造 TLS ClientHello，以绕过允许特定主机名的 SNI 过滤。&lt;/p&gt;&lt;p&gt;仅适用于客户端。需要提权 (root / 管理员)。在 Windows ARM64 上不被支持。macOS 上不支持 &amp;quot;wrong-timestamp&amp;quot;。&lt;/p&gt;&lt;p&gt;留空则继承来自“预设设置”的全局值。&lt;/p&gt;&lt;/body&gt;&lt;/html&gt;</translation>
    </message>
    <message>
        <source>Spoof SNI</source>
        <translation>伪装 SNI</translation>
    </message>
    <message>
        <source>Allowed SNI to forge</source>
        <translation>允许 SNI 伪装</translation>
    </message>
    <message>
        <source>Method</source>
        <translation>方法</translation>
    </message>
    <message>
        <source>&lt;html&gt;&lt;head/&gt;&lt;body&gt;&lt;h4 style=&quot; margin-top:12px; margin-bottom:12px; margin-left:0px; margin-right:0px; -qt-block-indent:0; text-indent:0px;&quot;&gt;&lt;a name=&quot;query_server_name&quot;/&gt;&lt;span style=&quot; font-size:medium; font-weight:700;&quot;&gt;q&lt;/span&gt;&lt;span style=&quot; font-size:medium; font-weight:700;&quot;&gt;uery_server_name in sing-box&lt;/span&gt;&lt;/h4&gt;&lt;/body&gt;&lt;/html&gt;</source>
        <translation>&lt;html&gt;&lt;head/&gt;&lt;body&gt;&lt;h4 style=&quot; margin-top:12px; margin-bottom:12px; margin-left:0px; margin-right:0px; -qt-block-indent:0; text-indent:0px;&quot;&gt;&lt;a name=&quot;query_server_name&quot;/&gt;&lt;span style=&quot; font-size:medium; font-weight:700;&quot;&gt;sing-box 中的 query_server_name (查询服务器)&lt;/span&gt;&lt;/h4&gt;&lt;/body&gt;&lt;/html&gt;</translation>
    </message>
    <message>
        <source>ECH Query Name / Resolver</source>
        <translation>ECH 查询名称/解析器</translation>
    </message>
    <message>
        <source>domain or domain+resolver</source>
        <translation>域名 或 域名+解析器</translation>
    </message>
    <message>
        <source>ECH Config</source>
        <translation>ECH 配置</translation>
    </message>
    <message>
        <source>Not Set</source>
        <translation>未设置</translation>
    </message>
    <message>
        <source>&lt;html&gt;&lt;head/&gt;&lt;body&gt;&lt;p&gt;SHA-256 of the server&apos;s whole certificate in base64, one per line (pcs in share links). A match replaces CA verification, so a self-signed certificate works.&lt;/p&gt;&lt;p&gt;Cannot be combined with a custom Certificate.&lt;/p&gt;&lt;/body&gt;&lt;/html&gt;</source>
        <translation>&lt;html&gt;&lt;head/&gt;&lt;body&gt;&lt;p&gt;服务器的整个证书（Base64 编码）的 SHA-256，每行一个（共享链接中的 pcs）。一次匹配可以替代 CA 验证，所以自签证书也能用。&lt;/p&gt;&lt;p&gt;不能和自定义证书一起使用。&lt;/p&gt;&lt;/body&gt;&lt;/html&gt;</translation>
    </message>
    <message>
        <source>Certificate SHA256</source>
        <translation>证书 SHA256</translation>
    </message>
    <message>
        <source>&lt;html&gt;&lt;head/&gt;&lt;body&gt;&lt;p&gt;SHA-256 of the server certificate&apos;s public key in base64, one per line. A match replaces CA verification and survives renewals that keep the key.&lt;/p&gt;&lt;p&gt;Cannot be combined with a custom Certificate.&lt;/p&gt;&lt;/body&gt;&lt;/html&gt;</source>
        <translation>&lt;html&gt;&lt;head/&gt;&lt;body&gt;&lt;p&gt;服务器证书公钥（Base64 编码）的 SHA-256 值，每行一个。一次匹配可以替代 CA 验证，并在保留密钥的续期中有效。&lt;/p&gt;&lt;p&gt;不能和自定义证书一起使用。&lt;/p&gt;&lt;/body&gt;&lt;/html&gt;</translation>
    </message>
    <message>
        <source>Public Key SHA256</source>
        <translation>公钥 SHA256</translation>
    </message>
    <message>
        <source>Client Certificate</source>
        <translation>客户端证书</translation>
    </message>
    <message>
        <source>Client Key</source>
        <translation>客户端钥匙</translation>
    </message>
    <message>
        <source>QUIC Parameters</source>
        <translation>QUIC 参数</translation>
    </message>
    <message>
        <source>Empty fields fall back to the global values in Preset Settings.</source>
        <translation>空字段回退到“预设设置”中的全局值。</translation>
    </message>
    <message>
        <source>Idle Timeout</source>
        <translation>空闲超时</translation>
    </message>
    <message>
        <source>Keep Alive Period</source>
        <translation>保活周期</translation>
    </message>
    <message>
        <source>Stream Receive Window</source>
        <translation>流接收窗口</translation>
    </message>
    <message>
        <source>Connection Receive Window</source>
        <translation>连接接收窗口</translation>
    </message>
    <message>
        <source>Max Concurrent Streams</source>
        <translation>最大并发流数</translation>
    </message>
    <message>
        <source>Initial Packet Size</source>
        <translation>初始数据包大小</translation>
    </message>
    <message>
        <source>Disable Path MTU Discovery</source>
        <translation>禁用路径 MTU 发现</translation>
    </message>
    <message>
        <source>AnyTLS does not support TCP Fast Open, so it is turned off when the profile starts.</source>
        <translation>AnyTLS 不支持 TCP Fast Open（快速开启），所以当配置档启动时它会被关闭。</translation>
    </message>
    <message>
        <source>TCP Fast Open is ignored while the custom TLS fragment is on.</source>
        <translation>当自定义 TLS 分片开启时，TCP Fast Open （快速开启）会被忽略。</translation>
    </message>
    <message>
        <source>A dhcp:// resolver cannot fetch the ECH config. Use a udp://, tls:// or https:// address.</source>
        <translation>dhcp:// 解析器无法获取 ECH 配置。请使用 udp://、tls:// 或 https:// 地址。</translation>
    </message>
    <message>
        <source>Inherit from group</source>
        <translation>从分组中继承</translation>
    </message>
    <message>
        <source>Own address</source>
        <translation>自己的地址</translation>
    </message>
    <message>
        <source>IP list</source>
        <translation>IP 列表</translation>
    </message>
    <message>
        <source>Missing list</source>
        <translation>缺少的列表</translation>
    </message>
    <message>
        <source>Connects to %1 (%2).</source>
        <translation>连接到 %1 (%2)。</translation>
    </message>
    <message>
        <source>%1, so the profile&apos;s own address is used.</source>
        <translation>%1;因此使用该配置档的自身地址。</translation>
    </message>
    <message>
        <source>Already set</source>
        <translation>已设置</translation>
    </message>
    <message>
        <source>QUIC Opener</source>
        <translation>QUIC 前导包</translation>
    </message>
    <message>
        <source>&lt;html&gt;&lt;head/&gt;&lt;body&gt;&lt;p&gt;Sends one decoy packet before each new UDP flow, so networks that block QUIC by its first packet let the real handshake through. It helps HTTP/3 carriers (MASQUE, Hysteria2, TUIC). It does nothing when the profile dials through another profile.&lt;/p&gt;&lt;/body&gt;&lt;/html&gt;</source>
        <translation>&lt;html&gt;&lt;head/&gt;&lt;body&gt;&lt;p&gt;在每个新的 UDP 流之前发送一个诱饵包，使按首个数据包封锁 QUIC 的网络放行真正的握手。适用于基于 HTTP/3 的协议（MASQUE、Hysteria2、TUIC）。当此配置档通过另一个配置档拨号时无效。&lt;/p&gt;&lt;/body&gt;&lt;/html&gt;</translation>
    </message>
</context>
<context>
    <name>EditAnyTLS</name>
    <message>
        <source>Password</source>
        <translation>密码</translation>
    </message>
    <message>
        <source>Idle Session Check Interval</source>
        <translation>空闲会话检查时间间隔</translation>
    </message>
    <message>
        <source>Idle Session Timeout</source>
        <translation>空闲会话超时</translation>
    </message>
    <message>
        <source>Min Idle Session</source>
        <translation>最小空闲会话</translation>
    </message>
</context>
<context>
    <name>EditAutoSelector</name>
    <message>
        <source>Servers from</source>
        <translation>服务器来自</translation>
    </message>
    <message>
        <source>Only names matching</source>
        <translation>仅名称匹配</translation>
    </message>
    <message>
        <source>Share traffic between the best profiles</source>
        <translation>最优配置档间分享流量</translation>
    </message>
    <message>
        <source>Off by default: one profile carries everything and the rest stand by. Turn on to spread traffic over the profiles that are confirmed working.</source>
        <translation>默认关闭： 一个配置档承载一切流量而其余的待机。 开启（该选项）则流量分散到已确认正常运作的配置档上。</translation>
    </message>
    <message>
        <source>Preferred profile</source>
        <translation>首选配置档</translation>
    </message>
    <message>
        <source>Use automatic</source>
        <translation>使用自动(模式)</translation>
    </message>
    <message>
        <source>Stop preferring this profile and let the selector choose on measurements again.</source>
        <translation>停止首选这个配置档，并让选择器再次基于测量进行选择。</translation>
    </message>
    <message>
        <source>Advanced…</source>
        <translation>高级…</translation>
    </message>
    <message>
        <source>Everything here has a working default. Open it only if you want to tune how the selector measures and switches.</source>
        <translation>这里的一切设置都有正常工作的默认值。仅当你想调整选择器如何测量和切换时才打开它。</translation>
    </message>
    <message>
        <source>Which profiles to use</source>
        <translation>要使用的配置档</translation>
    </message>
    <message>
        <source>Only countries</source>
        <translation>仅指定国家</translation>
    </message>
    <message>
        <source>The groups whose profiles this selector picks from. Profiles from every checked group are pooled together, and each one is still wrapped in its own group&apos;s front and landing proxy, so it behaves as if started on its own. New servers added by a subscription update join automatically — you never have to edit this profile again.</source>
        <translation>这个选择器从中挑选配置档的分组。每个被选中组的配置档都会被汇总在一起，而且每个配置档仍然被封装在它所属组的前端和落地代理中，所以它的行为就像是自己独立启动的一样。通过订阅更新添加的新服务器会自动加入 - 你永远不需要再次编辑这个配置档。</translation>
    </message>
    <message>
        <source>leave empty to use every profile in the checked groups</source>
        <translation>留空表示使用选中分组中的每个配置档</translation>
    </message>
    <message>
        <source>Optional regular expression, matched anywhere in the profile name and ignoring case. Only profiles whose name matches are used.
Pick several: germany|nl
Leave some out: ^(?!.*(germany|🇩🇪)).*$ uses every name that contains neither.</source>
        <translation>可选的正则表达式：在配置档名称中的任意位置进行匹配，且不区分大小写。只有名称匹配的配置档才会被使用。
选取多个：germany|nl
排除部分名称：^(?!.*(germany|🇩🇪)).*$ 可匹配不包含这两项的所有名称。</translation>
    </message>
    <message>
        <source>e.g. DE,NL,FR or !RU</source>
        <translation>如 DE,NL,FR 或 !RU</translation>
    </message>
    <message>
        <source>Comma-separated country codes, empty means any. Put ! before a code to exclude that country instead, e.g. !RU uses every country but Russia. Uses the country found by the IP test, so profiles that were never IP-tested are skipped while this is set.</source>
        <translation>逗号间隔的国家/地区代码，留空表示任意。要排除某个国家/地区，在代码前加 !。例如，!RU 表示使用俄罗斯以外的所有国家/地区。使用由 IP 测试找到的国家/地区，因此，设定这个筛选条件后，未进行 IP 测试的配置档会被跳过。</translation>
    </message>
    <message>
        <source>Run the best</source>
        <translation>运行最优的</translation>
    </message>
    <message>
        <source> profiles</source>
        <translation> 个配置档</translation>
    </message>
    <message>
        <source>How many profiles are loaded into the running config. The selector switches between these instantly, with no reconnect. Higher costs more memory and startup time.</source>
        <translation>会加载到正在运行的配置中的配置档数量。选择器会在这些配置档间瞬时切换而无需重连。越高的量耗费越多的内存和启动时间。</translation>
    </message>
    <message>
        <source>Rank at most</source>
        <translation>最多排名</translation>
    </message>
    <message>
        <source>Hard ceiling on how many profiles are measured and kept in the ranked list. When every running profile dies, replacements are taken from this list.</source>
        <translation>在要被测量并保留在排名列表中的配置档的数量方面的硬性上限。当每个正在运行的配置档都失效时，将从这个列表中取得替代项。</translation>
    </message>
    <message>
        <source>Trust results for</source>
        <translation>信任结果维持</translation>
    </message>
    <message>
        <source> min</source>
        <translation> 分</translation>
    </message>
    <message>
        <source>always re-test</source>
        <translation>始终重新测试</translation>
    </message>
    <message>
        <source>URL test results this recent are reused instead of being measured again — including tests you ran yourself on the group, so starting the selector right after one does not repeat the work. Older results are treated as if the profile had never been tested. Set to zero to always run a fresh test.</source>
        <translation>近期的 URL 测试结果会被重复使用而不是再次测量 - 包括你本人对该组上运行过的测试，所以在完成一次测试后立即启动选择器，不用重复同样的工作。较旧的结果会被认为“该配置档从未被测试”。设为 0 表示始终运行一个全新测试。</translation>
    </message>
    <message>
        <source>Skip failed profiles</source>
        <translation>跳过失败的配置档</translation>
    </message>
    <message>
        <source>Keeps servers whose last test failed out of the pool, as long as that result is still within the trust window. Turn off if your tests are unreliable and you would rather let the selector decide for itself.</source>
        <translation>只要上次测试结果仍在可信（时间）窗口内，就会把测试失败的服务器从该（候选）池中剔除。如果测试不可靠，宁愿让选择器自行判断，则关闭这个选项。</translation>
    </message>
    <message>
        <source>Health checks</source>
        <translation>健康度检查</translation>
    </message>
    <message>
        <source>Keep ready</source>
        <translation>保持就绪</translation>
    </message>
    <message>
        <source>How many profiles are kept confirmed working at all times, so a failure can be covered instantly. These are also the ones traffic is shared between when balancing is on.</source>
        <translation>始终保持被确认正常运作的配置档数量，以便出现故障时可瞬时（替补）恢复。开启（负载）均衡时这些（配置档）也会分享流量。</translation>
    </message>
    <message>
        <source>Check closely</source>
        <translation>仔细检查</translation>
    </message>
    <message>
        <source>The top-ranked profiles are re-checked every interval. The rest are still checked, just spread over the slower full sweep.</source>
        <translation>每隔指定时间间隔，排名靠前的配置档会进行一次重新检查。其余的仍会被检查，只是会分散到较慢的完整扫描流程中进行。</translation>
    </message>
    <message>
        <source>Check interval</source>
        <translation>检查间隔</translation>
    </message>
    <message>
        <source> s</source>
        <translation> 秒</translation>
    </message>
    <message>
        <source>How often the closely-checked profiles are measured again.</source>
        <translation>仔细检查的配置档重复测量的频繁程度。</translation>
    </message>
    <message>
        <source>Full sweep every</source>
        <translation>完整扫描每隔</translation>
    </message>
    <message>
        <source>How long one pass over every running profile takes. Checks are spread evenly across it, so a large pool never causes a burst of traffic.</source>
        <translation>遍历每个运行中的配置档一次所需时间。检查会在其中均匀分布，因此不会因一个大型（配置档）池而导致流量的突发。</translation>
    </message>
    <message>
        <source>Watch profile in use</source>
        <translation>监视使用中的配置档</translation>
    </message>
    <message>
        <source>How often the profile currently carrying your traffic is checked on its own. Keep this short: a server can stop working while still accepting connections, and nothing else would notice until the next full check.</source>
        <translation>当前承载你的流量的配置档多久会被自行检查一次。 请注意： 服务器可能在其仍能接受连接的情况下停止运作，并且直至下一次完整检查前都不会收到任何其他情况的通知。</translation>
    </message>
    <message>
        <source>Samples kept</source>
        <translation>保留样本</translation>
    </message>
    <message>
        <source>Ranking uses the average and the jitter over this many recent checks, so a single bad result cannot demote a good server.</source>
        <translation>排名使用这几次近期检查的均值和抖动(Jitter)，因此单次的异常结果不会降级一个良好的服务器。</translation>
    </message>
    <message>
        <source>Switching</source>
        <translation>切换</translation>
    </message>
    <message>
        <source>Switch tolerance</source>
        <translation>切换容差</translation>
    </message>
    <message>
        <source> ms</source>
        <translation> 毫秒</translation>
    </message>
    <message>
        <source>Another profile must be at least this much faster before the selector moves to it. Higher values mean fewer switches.</source>
        <translation>在选择器移到另一个配置档前，它必须至少比当前的快这个量。越高的值意味着越少的切换次数。</translation>
    </message>
    <message>
        <source>Maximum latency</source>
        <translation>最大延迟</translation>
    </message>
    <message>
        <source>no limit</source>
        <translation>无限制</translation>
    </message>
    <message>
        <source>Profiles slower than this are never selected, even if nothing better is available.</source>
        <translation>比这个慢的配置档永远不会被选择，即使没有更好的项可用。</translation>
    </message>
    <message>
        <source>Failover attempts</source>
        <translation>故障切换尝试次数</translation>
    </message>
    <message>
        <source>When the chosen profile fails to connect, how many others to try immediately before the app sees an error.</source>
        <translation>当所选的配置档连接失败时，在应用看到错误前，立即尝试多少个其他配置档。</translation>
    </message>
    <message>
        <source>Drop connections on switch</source>
        <translation>切换时断开连接</translation>
    </message>
    <message>
        <source>Applies when the selector switches because a profile got worse or failed. On: everything moves to the new profile at once. Off: current downloads finish on the old one. Balance rotations never drop connections either way.</source>
        <translation>当选择器因配置档变差或失败而切换时应用。开启：一切内容会一次性移到新配置档。关闭：当前下载会在旧的配置档上完成。 无论何种方式，均衡轮换都不会断开连接。</translation>
    </message>
    <message>
        <source>Load balancing</source>
        <translation>负载均衡</translation>
    </message>
    <message>
        <source>Mode</source>
        <translation>模式</translation>
    </message>
    <message>
        <source>How traffic is spread over the ready profiles.</source>
        <translation>流量如何在已准备好的配置档间分配。</translation>
    </message>
    <message>
        <source>Rotate every</source>
        <translation>轮换每隔</translation>
    </message>
    <message>
        <source>How long the selector stays on one profile before moving to the next ready one.</source>
        <translation>选择器在移到下一个准备好的配置档之前会停留在当前配置档上的时长。</translation>
    </message>
    <message>
        <source>Test endpoints</source>
        <translation>测试端点</translation>
    </message>
    <message>
        <source>Test URL</source>
        <translation>测试 URL</translation>
    </message>
    <message>
        <source>empty = use the global latency test URL</source>
        <translation>空 = 使用全局延迟测试 URL</translation>
    </message>
    <message>
        <source>Fetched through each profile to measure it.</source>
        <translation>通过每个配置档进行抓取以进行测量。</translation>
    </message>
    <message>
        <source>Connectivity URL</source>
        <translation>连通性 URL</translation>
    </message>
    <message>
        <source>optional — inherits the Direct Test URL</source>
        <translation>可选 - 继承直连测试 URL</translation>
    </message>
    <message>
        <source>Fetched WITHOUT the proxy to tell a dead internet connection apart from dead servers, and to notice the moment it comes back. Leave empty to inherit the Direct Test URL from Basic Settings, and empty there too to rely on the operating system&apos;s network state. A URL that turns out to be blocked can only delay a resume by a few minutes; it can never keep the selector paused.</source>
        <translation>“不用”代理获取，以区分互联网连接中断和服务器故障，并在恢复时立即觉察。留空则继承「基本设置」中的「直连测试 URL」，而且若那里也为空则依赖操作系统的网络状态。若 URL 后来发现被拦截，最多只会使恢复延迟数分钟，绝不会使选择器一直处于暂停状态。</translation>
    </message>
    <message>
        <source>Rotate on a timer (keeps sessions stable)</source>
        <translation>按计时器轮换(保持会话稳定)</translation>
    </message>
    <message>
        <source>Per connection (widest spread)</source>
        <translation>按连接(分布最广泛分散)</translation>
    </message>
    <message>
        <source>Advanced ▴</source>
        <translation>高级 ▴</translation>
    </message>
    <message>
        <source>Name cannot be empty.</source>
        <translation>名称不能为空。</translation>
    </message>
    <message>
        <source>%1 — chosen by you, so the selector stays on it while it works</source>
        <translation>%1 — 由你选择，只要它正常工作，选择器在继续使用它</translation>
    </message>
    <message>
        <source>The best profile carries all traffic; the other ready profiles are kept verified so one can take over instantly.</source>
        <translation>最好的配置档承载所有流量；其他的已就绪配置档会保持经过验证状态，以便可以（在需要时）瞬时接管。</translation>
    </message>
    <message>
        <source>New connections move to another good profile on each rotation; open ones are left alone and finish where they started. Traffic is counted exactly.</source>
        <translation>每次轮换时，新的连接会移到另一个好的配置档；已打开的（连接）会保持不变并在其开始的位置（配置档）上完成工作。流量会精确统计。</translation>
    </message>
    <message>
        <source>Every new connection may take a different profile. Widest spread, but your exit IP changes mid-session and per-profile traffic becomes approximate.</source>
        <translation>每个新的连接可采用不同的配置档。分布最广，但是你的出口 IP 会在会话中途更改，并且按配置档统计的流量会变成近似值。</translation>
    </message>
    <message>
        <source>Check at least one group for this auto selector to track.</source>
        <translation>至少为这个自动选择器选择一个分组进行跟踪。</translation>
    </message>
    <message>
        <source>%1 of %2 profiles in the checked groups can be used; %3 would run.</source>
        <translation>勾选分组中共 %2 个配置档，其中可使用 %1 个；将运行 %3 个。</translation>
    </message>
    <message numerus="yes">
        <source>%n tracked group(s) no longer exist and are ignored; saving removes them.</source>
        <translation>
            <numerusform>%n 个跟踪的分组已不存在并被忽略；保存时移除它们。</numerusform>
        </translation>
    </message>
    <message>
        <source>%1 %2</source>
        <translation>%1 %2</translation>
    </message>
    <message>
        <source>Skipped: %1.</source>
        <translation>已跳过: %1。</translation>
    </message>
    <message>
        <source>, </source>
        <translation>， </translation>
    </message>
    <message>
        <source>Every profile&apos;s last test failed, which usually means the network was down rather than the servers, so all %1 are kept and will be re-checked.</source>
        <translation>每个配置档的上次测试均失败，这通常意味着网络中断而不是服务器宕机，因此将保留私有 %1 个配置档并将会重新检查。</translation>
    </message>
    <message>
        <source>More than %1 profiles match, so only the best-ranked ones are kept.</source>
        <translation>超过 %1 个配置档匹配，因此只保留排名最优的。</translation>
    </message>
    <message>
        <source>%1 have a recent test result that will be reused.</source>
        <translation>%1 个有将被复用的近期测试结果。</translation>
    </message>
    <message>
        <source>The rest will be measured as the selector starts: it runs as soon as enough profiles work, and switches to the full ranking once all are measured.</source>
        <translation>其余配置档会随着选择器启动后继续测量：一旦有足够多的配置档可用，选择器就会运行；待全部配置档测量完毕后，再切换为完整排名。</translation>
    </message>
</context>
<context>
    <name>EditChain</name>
    <message>
        <source>Traffic order is from top to bottom</source>
        <translation>流量顺序是由上到下 (最底一个为流量的出口)</translation>
    </message>
    <message>
        <source>Select Profile</source>
        <translation>选择配置档</translation>
    </message>
    <message>
        <source>A custom %1 config cannot be a hop; only custom outbounds can.</source>
        <translation>自定义 %1 配置不能是一个跳点；仅自定义出站可以。</translation>
    </message>
    <message>
        <source>Only one Extra Core profile can be in a chain.</source>
        <translation>链中只能有一个「额外核心」配置档。</translation>
    </message>
    <message>
        <source>Only one custom Xray config can be in a chain.</source>
        <translation>链中只能有一个自定义 Xray 配置。</translation>
    </message>
    <message>
        <source>An Extra Core profile and a custom Xray config cannot be in the same chain.</source>
        <translation>「额外核心」配置档和自定义 Xray 配置不能在同一链中。</translation>
    </message>
    <message>
        <source>A custom Xray config cannot share a chain with other Xray profiles.</source>
        <translation>自定义 Xray 配置不能与其它 Xray 配置档共享链路。</translation>
    </message>
    <message>
        <source>An Extra Core profile can only be the final hop; move it to the top of the list.</source>
        <translation>「额外核心」配置档只可以是最后一跳；把它移到列表的顶部。</translation>
    </message>
    <message>
        <source>A custom Xray config can only be the final hop; move it to the top of the list.</source>
        <translation>Xray 配置只可以是最后一跳；把它移到列表的顶部。</translation>
    </message>
    <message>
        <source>The chain switches between %1 and Xray too often; keep the Xray hops together, between %1 hops.</source>
        <translation>该链在 %1 和 Xray 之间切换太频繁；Xray 跳点保持在一起，介于 %1 跳点之间。</translation>
    </message>
    <message>
        <source>Name cannot be empty.</source>
        <translation>名称不能为空。</translation>
    </message>
    <message>
        <source>Hops</source>
        <translation>跳点</translation>
    </message>
    <message numerus="yes">
        <source>%n hop(s) no longer exist or cannot be hops, so they were left out; saving removes them.</source>
        <translation>
            <numerusform>%n 个跳点不再存在或者不能是跳点，所以它们被排除；保存时移除它们。</numerusform>
        </translation>
    </message>
    <message>
        <source>Hop %1 no longer exists; replace or remove it.</source>
        <translation>跳点 %1 不再存在；替换或移除它。</translation>
    </message>
    <message>
        <source>Add at least one profile to the chain.</source>
        <translation>添加至少一个配置档到链。</translation>
    </message>
    <message>
        <source>Tests each hop on its own and through every hop above it, without saving the chain</source>
        <translation>单独测试每个跳点，并经由其上方的所有跳点测试，不保存该链</translation>
    </message>
    <message>
        <source>Test Hops</source>
        <translation>测试跳点</translation>
    </message>
    <message>
        <source>A chain can hold only one auto selector.</source>
        <translation>一条链只能包含一个自动选择器。</translation>
    </message>
    <message>
        <source>An auto selector cannot share a chain with an Extra Core profile or a custom config.</source>
        <translation>自动选择器不能与「额外核心」配置档或自定义配置位于同一链中。</translation>
    </message>
    <message>
        <source>Connect OK</source>
        <translation>连接通过</translation>
    </message>
    <message>
        <source>%1 ms</source>
        <translation>%1 ms</translation>
    </message>
    <message>
        <source>On its own: %1</source>
        <translation>单独：%1</translation>
    </message>
    <message>
        <source>On its own: unavailable</source>
        <translation>单独：不可用</translation>
    </message>
    <message>
        <source>Invalid</source>
        <translation>无效</translation>
    </message>
    <message>
        <source>This profile is not valid; the log has the details.</source>
        <translation>此配置档无效；详情见日志。</translation>
    </message>
    <message>
        <source>Works.</source>
        <translation>可用。</translation>
    </message>
    <message>
        <source>Works through every hop above it.</source>
        <translation>经由其上方的所有跳点均可用。</translation>
    </message>
    <message>
        <source>Not reached</source>
        <translation>未到达</translation>
    </message>
    <message>
        <source>A hop above already fails, so this says nothing about this hop.</source>
        <translation>上方已有跳点失败，因此无法据此判断此跳点。</translation>
    </message>
    <message>
        <source>Fails through the hops above</source>
        <translation>经由上方跳点失败</translation>
    </message>
    <message>
        <source>Works on its own (%1), but not through the hops above it: the hop above may not reach this server, or may not carry its protocol (UDP, for example).</source>
        <translation>单独可用 (%1)，但经由上方跳点不可用：上方的跳点可能无法访问此服务器，或无法承载其协议（例如 UDP）。</translation>
    </message>
    <message>
        <source>Unavailable</source>
        <translation>不可用</translation>
    </message>
    <message>
        <source>Tested on its own only: a hop above is not valid.</source>
        <translation>仅单独测试：上方有跳点无效。</translation>
    </message>
    <message>
        <source>Not tested</source>
        <translation>未测试</translation>
    </message>
    <message>
        <source>The test was stopped.</source>
        <translation>测试已停止。</translation>
    </message>
    <message>
        <source>The chain breaks at hop %1, %2.</source>
        <translation>链在第 %1 个跳点 %2 处中断。</translation>
    </message>
    <message>
        <source>Every hop works, and so does the group&apos;s landing proxy after them (%1).</source>
        <translation>所有跳点均可用，其后的分组落地代理也可用 (%1)。</translation>
    </message>
    <message>
        <source>Every hop works, but the group&apos;s landing proxy fails through them.</source>
        <translation>所有跳点均可用，但分组的落地代理经由它们失败。</translation>
    </message>
    <message>
        <source>Every hop works, but the group&apos;s landing proxy is unavailable.</source>
        <translation>所有跳点均可用，但分组的落地代理不可用。</translation>
    </message>
    <message>
        <source>Every hop works through the ones above it.</source>
        <translation>每个跳点经由其上方的跳点均可用。</translation>
    </message>
    <message>
        <source>Testing…</source>
        <translation>正在测试…</translation>
    </message>
    <message>
        <source>Testing each hop on its own and through the hops above it…</source>
        <translation>正在单独测试每个跳点，并经由其上方的跳点测试…</translation>
    </message>
    <message>
        <source>The hops changed during the test; test again.</source>
        <translation>测试期间跳点发生了变化；请重新测试。</translation>
    </message>
    <message>
        <source>Another test is running; try again when it finishes.</source>
        <translation>另一个测试正在运行；请在其结束后重试。</translation>
    </message>
</context>
<context>
    <name>EditCustom</name>
    <message>
        <source>Core</source>
        <translation>核心</translation>
    </message>
    <message>
        <source>Json Editor</source>
        <translation>JSON 编辑器</translation>
    </message>
    <message>
        <source>Outbound JSON, please read the documentation.</source>
        <translation>出站 JSON，详情请阅读文档。</translation>
    </message>
    <message>
        <source>Xray outbound JSON, please read the Xray documentation.</source>
        <translation>Xray Outbound JSON，详情请阅读 Xray 文档。</translation>
    </message>
    <message>
        <source>Xray full config (Throne adds a socks inbound and uses sing-box for tun/routing).</source>
        <translation>Xray 完整配置 (Throne 添加一个 socks inbound (套接字入站)并把 sing-box 用于 tun / 路由)。</translation>
    </message>
    <message>
        <source>Please fill the complete config.</source>
        <translation>请填写完整的配置。</translation>
    </message>
    <message>
        <source>Name cannot be empty.</source>
        <translation>名称不能为空。</translation>
    </message>
    <message>
        <source>Config</source>
        <translation>配置</translation>
    </message>
</context>
<context>
    <name>EditDirect</name>
    <message>
        <source>Use the Advanced section to specify bind options</source>
        <translation>使用“高级”部分指定绑定选项</translation>
    </message>
</context>
<context>
    <name>EditDnstt</name>
    <message>
        <source>Mode</source>
        <translation>模式</translation>
    </message>
    <message>
        <source>Domain</source>
        <translation>域名</translation>
    </message>
    <message>
        <source>Public Key</source>
        <translation>公钥</translation>
    </message>
    <message>
        <source>Record Type</source>
        <translation>记录类型</translation>
    </message>
    <message>
        <source>Client ID Size</source>
        <translation>客户端 ID 大小</translation>
    </message>
    <message>
        <source>Max QNAME Length</source>
        <translation>最大 QNAME 长度</translation>
    </message>
    <message>
        <source>Payload per Query</source>
        <translation>每个查询的载荷</translation>
    </message>
    <message>
        <source>Resolvers</source>
        <translation>解析器</translation>
    </message>
    <message>
        <source>Strategy</source>
        <translation>策略</translation>
    </message>
    <message>
        <source>Retry truncated replies over TCP</source>
        <translation>通过 TCP 重试被截断的响应</translation>
    </message>
    <message>
        <source>Backend</source>
        <translation>后端</translation>
    </message>
    <message>
        <source>Type</source>
        <translation>类型</translation>
    </message>
    <message>
        <source>Username</source>
        <translation>用户名</translation>
    </message>
    <message>
        <source>Password</source>
        <translation>密码</translation>
    </message>
    <message>
        <source>UDP over TCP</source>
        <translation>基于 TCP 的 UDP</translation>
    </message>
    <message>
        <source>Encryption</source>
        <translation>加密</translation>
    </message>
    <message>
        <source>User</source>
        <translation>用户</translation>
    </message>
    <message>
        <source>Private Key</source>
        <translation>私钥</translation>
    </message>
    <message>
        <source>Private Key Path</source>
        <translation>私钥路径</translation>
    </message>
    <message>
        <source>Choose File</source>
        <translation>选择文件</translation>
    </message>
    <message>
        <source>Private Key Password</source>
        <translation>私钥密码</translation>
    </message>
    <message>
        <source>Host Keys</source>
        <translation>主机钥匙</translation>
    </message>
    <message>
        <source>One per line</source>
        <translation>每行一个</translation>
    </message>
    <message>
        <source>Host Key Algorithms</source>
        <translation>主机钥匙算法</translation>
    </message>
    <message>
        <source>Client Version</source>
        <translation>客户端版本</translation>
    </message>
    <message>
        <source>Advanced</source>
        <translation>高级</translation>
    </message>
    <message>
        <source>Idle Timeout</source>
        <translation>空闲超时</translation>
    </message>
    <message>
        <source>Keepalive Interval</source>
        <translation>保活时间间隔</translation>
    </message>
    <message>
        <source>Queries per Second</source>
        <translation>每秒查询数</translation>
    </message>
    <message>
        <source>VayDNS (dnstt-compatible)</source>
        <translation>VayDNS (兼容 dnstt)</translation>
    </message>
    <message>
        <source>Default</source>
        <translation>默认</translation>
    </message>
    <message>
        <source>None (chain only)</source>
        <translation>无 (仅链式)</translation>
    </message>
    <message>
        <source>Tunnel data each query carries; a longer domain leaves less room.</source>
        <translation>每个查询携带的隧道数据量；域名越长剩余空间越少。</translation>
    </message>
    <message>
        <source>%1 bytes</source>
        <translation>%1 字节</translation>
    </message>
</context>
<context>
    <name>EditExtraCore</name>
    <message>
        <source>Socks address</source>
        <translation>Socks 地址</translation>
    </message>
    <message>
        <source>127.0.0.1</source>
        <translation></translation>
    </message>
    <message>
        <source>Socks port</source>
        <translation>Socks 端口</translation>
    </message>
    <message>
        <source>1080</source>
        <translation></translation>
    </message>
    <message>
        <source>Core path</source>
        <translation>核心路径</translation>
    </message>
    <message>
        <source>Choose from file</source>
        <translation>从文件中选择</translation>
    </message>
    <message>
        <source>&lt;html&gt;&lt;head/&gt;&lt;body&gt;&lt;p&gt;args to pass to the executable. You can insert a single %s and it will be replaced with the path to a file containing the config entered below.&lt;/p&gt;&lt;/body&gt;&lt;/html&gt;</source>
        <translation>&lt;html&gt;&lt;head/&gt;&lt;body&gt;&lt;p&gt;传递到可执行文件的参数。您可以插入单个 %s，它将被替换为包含下面输入的配置的文件的路径。&lt;/p&gt;&lt;/body&gt;&lt;/html&gt;</translation>
    </message>
    <message>
        <source>Args</source>
        <translation>参数</translation>
    </message>
    <message>
        <source>run -confPath %s</source>
        <translation>run -confPath %s</translation>
    </message>
    <message>
        <source>&lt;html&gt;&lt;head/&gt;&lt;body&gt;&lt;p&gt;contents of the config file that will be passed to the extra core process if %s is specified in the args&lt;/p&gt;&lt;/body&gt;&lt;/html&gt;</source>
        <translation>&lt;html&gt;&lt;head/&gt;&lt;body&gt;&lt;p&gt;若在参数中指定了 %s，则配置文件的内容将传递给额外核心进程&lt;/p&gt;&lt;/body&gt;&lt;/html&gt;</translation>
    </message>
    <message>
        <source>Config</source>
        <translation>配置</translation>
    </message>
    <message>
        <source>No logs</source>
        <translation>无日志</translation>
    </message>
</context>
<context>
    <name>EditHttp</name>
    <message>
        <source>Username</source>
        <translation>用户名</translation>
    </message>
    <message>
        <source>Password</source>
        <translation>密码</translation>
    </message>
</context>
<context>
    <name>EditHysteria</name>
    <message>
        <source>Protocol Version</source>
        <translation>协议版本</translation>
    </message>
    <message>
        <source>Server Ports</source>
        <translation>服务器端口</translation>
    </message>
    <message>
        <source>Hop Interval</source>
        <translation>跃点间隔</translation>
    </message>
    <message>
        <source>Hop Interval Max</source>
        <translation>最大跳跃间隔</translation>
    </message>
    <message>
        <source>When set, each hop waits a random duration between Hop Interval and this value. Requires Hop Interval.</source>
        <translation>在设置时，每次跳跃都会等待一个随机时间（介于”跳跃间隔“和这个值之间）。需要”跳跃间隔“。</translation>
    </message>
    <message>
        <source>Up Mbps</source>
        <translation>上行 (Mbps)</translation>
    </message>
    <message>
        <source>Down Mbps</source>
        <translation>下行 (Mbps)</translation>
    </message>
    <message>
        <source>BBR Profile</source>
        <translation>BBR 配置档</translation>
    </message>
    <message>
        <source>Congestion control aggressiveness. Leave empty to use the default (standard).</source>
        <translation>拥塞控制的激进程度。留空表示使用默认（标准）设置。</translation>
    </message>
    <message>
        <source>Authentication Type</source>
        <translation>认证类型</translation>
    </message>
    <message>
        <source>Authentication Payload</source>
        <translation>认证负载</translation>
    </message>
    <message>
        <source>Obfuscation</source>
        <translation>混淆</translation>
    </message>
    <message>
        <source>None</source>
        <translation>无</translation>
    </message>
    <message>
        <source>Obfuscation Password</source>
        <translation>混淆密码</translation>
    </message>
    <message>
        <source>Minimum Packet Size</source>
        <translation>最小数据包大小</translation>
    </message>
    <message>
        <source>Maximum Packet Size</source>
        <translation>最大数据包大小</translation>
    </message>
    <message>
        <source>Recv window conn</source>
        <translation>接收窗口连接</translation>
    </message>
    <message>
        <source>Recv window</source>
        <translation>接收窗口</translation>
    </message>
    <message>
        <source>Disable Mtu Discovery</source>
        <translation>禁用 MTU 探索</translation>
    </message>
    <message>
        <source>Password</source>
        <translation>密码</translation>
    </message>
    <message>
        <source>Parroting makes the QUIC handshake look like Chrome&apos;s. On by default.
It overrides idle timeout, max concurrent streams,
initial packet size and the initial receive windows.</source>
        <translation>模仿功能使 QUIC 握手看起来类似 Chrome 的。默认开启。它会覆盖空闲超时、最大并发流数、初始数据包大小和初始接收窗口。</translation>
    </message>
    <message>
        <source>Disable Chrome Parrot</source>
        <translation>禁用 Chrome 模仿</translation>
    </message>
    <message>
        <source>Reach a Hysteria2 server behind NAT through a Hysteria Realm
rendezvous service. Replaces the server address and port.</source>
        <translation>通过 Hysteria Realm 会合服务访问位于 NAT 后的 Hysteria2 服务器。
替换服务器地址和端口。</translation>
    </message>
    <message>
        <source>Realm (NAT Traversal)</source>
        <translation>Realm (NAT 遍历)</translation>
    </message>
    <message>
        <source>Realm Options</source>
        <translation>Realm 选项</translation>
    </message>
    <message>
        <source>Obfuscation needs a password; without one, it is off.</source>
        <translation>混淆需要密码，没有它，则功能关闭。</translation>
    </message>
</context>
<context>
    <name>EditHysteriaRealm</name>
    <message>
        <source>Realm (NAT Traversal)</source>
        <translation>Realm (NAT 遍历)</translation>
    </message>
    <message>
        <source>The realm is a rendezvous service: it hands out the server&apos;s current public addresses so the connection can be hole-punched directly.</source>
        <translation>Realm (领域) 是一个 Rendezvous (会面)服务：它交出服务器的当前公共地址，这样连接就可以直接洞穿而过。</translation>
    </message>
    <message>
        <source>Realm URL</source>
        <translation>Realm 网址</translation>
    </message>
    <message>
        <source>Realm ID</source>
        <translation>Realm ID</translation>
    </message>
    <message>
        <source>The slot the target Hysteria2 server registered on the realm.</source>
        <translation>目标 Hysteria2 服务器在该 realm 上注册的槽位。</translation>
    </message>
    <message>
        <source>Realm Token</source>
        <translation>Realm 令牌</translation>
    </message>
    <message>
        <source>STUN Servers</source>
        <translation>STUN 服务器</translation>
    </message>
    <message>
        <source>Comma separated host or host:port entries used to discover this client&apos;s public addresses.</source>
        <translation>用于发现这个客户端的公网地址的逗号间隔的“主机”或“主机:端口”条目。</translation>
    </message>
    <message>
        <source>IP Version</source>
        <translation>IP 版本</translation>
    </message>
    <message>
        <source>Maintain a UDP port mapping on the local gateway via UPnP or NAT-PMP.
Improves hole-punching reliability. Requires IPv4.</source>
        <translation>通过 UPnP 或 NAT-PMP 在本地网关上保持 UDP 端口映射。
提高洞穿的可靠性。需要使用 IPv4。</translation>
    </message>
    <message>
        <source>Port Mapping</source>
        <translation>端口映射</translation>
    </message>
    <message>
        <source>Mapping Timeout</source>
        <translation>映射超时</translation>
    </message>
    <message>
        <source>Mapping Lifetime</source>
        <translation>映射生命周期</translation>
    </message>
</context>
<context>
    <name>EditJuicity</name>
    <message>
        <source>Password</source>
        <translation>密码</translation>
    </message>
    <message>
        <source>UUID</source>
        <translation>UUID</translation>
    </message>
    <message>
        <source>Cert Chain SHA256</source>
        <translation>证书链 SHA256</translation>
    </message>
    <message>
        <source>Certificate chain hash printed by juicity-server; replaces CA verification, even with Allow insecure</source>
        <translation>juicity-server 输出的证书链哈希；即使启用了“允许不安全”，也会替代 CA 验证</translation>
    </message>
</context>
<context>
    <name>EditMasque</name>
    <message>
        <source>Base64 DER of the enrolled ECDSA P-256 key; it signs the TLS client certificate.</source>
        <translation>已注册的(enrolled) ECDSA P-256 钥匙的 Base64 DER 编码；它用于签署 TLS 客户端证书。</translation>
    </message>
    <message>
        <source>Private Key</source>
        <translation>私钥</translation>
    </message>
    <message>
        <source>Base64 DER of the server&apos;s public key. When set, it pins the server certificate, even with Allow insecure.</source>
        <translation>服务器公钥的 Base64 DER 编码。设置时，即使启用了“允许不安全(连接)”，也会固定服务器证书。</translation>
    </message>
    <message>
        <source>Peer Public Key</source>
        <translation>对端公钥</translation>
    </message>
    <message>
        <source>Comma separated list of the tunnel&apos;s own addresses, e.g. 172.16.0.2/32,2606:4700::1/128</source>
        <translation>隧道自身地址的逗号间隔列表，如 172.16.0.2/32,2606:4700::1/128</translation>
    </message>
    <message>
        <source>Local Address</source>
        <translation>本地地址</translation>
    </message>
    <message>
        <source>MTU</source>
        <translation>MTU</translation>
    </message>
    <message>
        <source>HTTP/3 runs over UDP; HTTP/2 runs over TLS on TCP and works where UDP is blocked.</source>
        <translation>HTTP/3 基于 UDP 运行；HTTP/2 基于 TCP 上的 TLS 运行并在 UDP 被阻断的情况下也可工作。</translation>
    </message>
    <message>
        <source>HTTP Version</source>
        <translation>HTTP 版本</translation>
    </message>
    <message>
        <source>HTTP/3 (fallback to HTTP/2)</source>
        <translation>HTTP/3 (回退到 HTTP/2)</translation>
    </message>
    <message>
        <source>HTTP/3 only</source>
        <translation>仅 HTTP/3</translation>
    </message>
    <message>
        <source>Register a new Cloudflare WARP device for MASQUE and fill the key, address and server fields.</source>
        <translation>为 MASQUE 注册新的 Cloudflare WARP 设备，并填写钥匙、地址和服务器字段。</translation>
    </message>
    <message>
        <source>Generate WARP identity</source>
        <translation>生成 WARP 身份标识</translation>
    </message>
    <message>
        <source>Generating identity...</source>
        <translation>正在生成身份标识...</translation>
    </message>
    <message>
        <source>Failed to generate WARP identity</source>
        <translation>生成 WARP 身份标识失败</translation>
    </message>
    <message>
        <source>Success!</source>
        <translation>成功!</translation>
    </message>
</context>
<context>
    <name>EditMasterDns</name>
    <message>
        <source>Domains</source>
        <translation>域名</translation>
    </message>
    <message>
        <source>One per line</source>
        <translation>每行一个</translation>
    </message>
    <message>
        <source>Mode</source>
        <translation>模式</translation>
    </message>
    <message>
        <source>Encryption</source>
        <translation>加密</translation>
    </message>
    <message>
        <source>Key</source>
        <translation>键</translation>
    </message>
    <message>
        <source>Resolvers</source>
        <translation>解析器</translation>
    </message>
    <message>
        <source>Strategy</source>
        <translation>策略</translation>
    </message>
    <message>
        <source>Transfer</source>
        <translation>传输</translation>
    </message>
    <message>
        <source>Duplication</source>
        <translation>重复</translation>
    </message>
    <message>
        <source>Setup Duplication</source>
        <translation>设置重复</translation>
    </message>
    <message>
        <source>Upload Compression</source>
        <translation>上传压缩</translation>
    </message>
    <message>
        <source>Download Compression</source>
        <translation>下载压缩</translation>
    </message>
    <message>
        <source>Base64-encoded replies</source>
        <translation>Base64编码的响应</translation>
    </message>
    <message>
        <source>MTU</source>
        <translation>MTU</translation>
    </message>
    <message>
        <source>Min Upload</source>
        <translation>最小上传</translation>
    </message>
    <message>
        <source>Max Upload</source>
        <translation>最大上传</translation>
    </message>
    <message>
        <source>Min Download</source>
        <translation>最小下载</translation>
    </message>
    <message>
        <source>Max Download</source>
        <translation>最大下载</translation>
    </message>
    <message>
        <source>Advanced</source>
        <translation>高级</translation>
    </message>
    <message>
        <source>MTU Test Retries</source>
        <translation>MTU 测试重试次数</translation>
    </message>
    <message>
        <source>MTU Test Timeout</source>
        <translation>MTU 测试超时</translation>
    </message>
    <message>
        <source>MTU Test Parallelism</source>
        <translation>MTU 测试并行度</translation>
    </message>
    <message>
        <source>Drop resolvers below the minimum MTU</source>
        <translation>丢弃低于最低 MTU 要求的 DNS 解析器</translation>
    </message>
    <message>
        <source>TCP / custom chain</source>
        <translation>TCP / 自定义链</translation>
    </message>
    <message>
        <source>Default</source>
        <translation>默认</translation>
    </message>
    <message>
        <source>Off</source>
        <translation>关闭</translation>
    </message>
</context>
<context>
    <name>EditMieru</name>
    <message>
        <source>Username</source>
        <translation>用户名</translation>
    </message>
    <message>
        <source>Password</source>
        <translation>密码</translation>
    </message>
    <message>
        <source>Transport</source>
        <translation>传输</translation>
    </message>
    <message>
        <source>Multiplexing</source>
        <translation>复用</translation>
    </message>
    <message>
        <source>Traffic Pattern</source>
        <translation>流量图案</translation>
    </message>
    <message>
        <source>Server Ports</source>
        <translation>服务器端口</translation>
    </message>
</context>
<context>
    <name>EditNaive</name>
    <message>
        <source>Password</source>
        <translation>密码</translation>
    </message>
    <message>
        <source>UDP over TCP</source>
        <translation>基于 TCP 的 UDP</translation>
    </message>
    <message>
        <source>Username</source>
        <translation>用户名</translation>
    </message>
    <message>
        <source>Congestion Control</source>
        <translation>拥塞控制</translation>
    </message>
    <message>
        <source>QUIC</source>
        <translation>QUIC</translation>
    </message>
    <message>
        <source>Extra Headers</source>
        <translation>额外标头</translation>
    </message>
    <message>
        <source>Headers sent to the proxy server, e.g. X-Username=&quot;user&quot; X-Password=&quot;pass&quot;</source>
        <translation>发送到代理服务器的标头，如 X-Username=&quot;user&quot; X-Password=&quot;pass&quot;</translation>
    </message>
    <message>
        <source>Insecure Concurrency</source>
        <translation>不安全并发</translation>
    </message>
    <message>
        <source>Parallel tunnel connections to the server, 0 to disable. More connections are easier to detect.</source>
        <translation>到服务器的并行隧道连接数，0 表示禁用。连接越多越容易被检测到。</translation>
    </message>
</context>
<context>
    <name>EditOpenConnect</name>
    <message>
        <source>&lt;html&gt;&lt;head/&gt;&lt;body&gt;&lt;p&gt;OpenConnect protocol flavor. Leave empty to use the core default (anyconnect).&lt;/p&gt;&lt;/body&gt;&lt;/html&gt;</source>
        <translation>&lt;html&gt;&lt;head/&gt;&lt;body&gt;&lt;p&gt;OpenConnect 协议类型。留空则使用核心的默认类型 (anyconnect)。&lt;/p&gt;&lt;/body&gt;&lt;/html&gt;</translation>
    </message>
    <message>
        <source>Flavor</source>
        <translation>类型</translation>
    </message>
    <message>
        <source>&lt;html&gt;&lt;head/&gt;&lt;body&gt;&lt;p&gt;Username used to fill matching authentication form fields.&lt;/p&gt;&lt;/body&gt;&lt;/html&gt;</source>
        <translation>&lt;html&gt;&lt;head/&gt;&lt;body&gt;&lt;p&gt;用于填写匹配的身份验证表单字段的用户名。&lt;/p&gt;&lt;/body&gt;&lt;/html&gt;</translation>
    </message>
    <message>
        <source>Username</source>
        <translation>用户名</translation>
    </message>
    <message>
        <source>&lt;html&gt;&lt;head/&gt;&lt;body&gt;&lt;p&gt;Password used to fill matching authentication form fields.&lt;/p&gt;&lt;/body&gt;&lt;/html&gt;</source>
        <translation>&lt;html&gt;&lt;head/&gt;&lt;body&gt;&lt;p&gt;用于填写匹配的身份验证表单字段的密码。&lt;/p&gt;&lt;/body&gt;&lt;/html&gt;</translation>
    </message>
    <message>
        <source>Password</source>
        <translation>密码</translation>
    </message>
    <message>
        <source>&lt;html&gt;&lt;head/&gt;&lt;body&gt;&lt;p&gt;Preselects a matching group, realm, domain or gateway choice when the flavor supports it.&lt;/p&gt;&lt;/body&gt;&lt;/html&gt;</source>
        <translation>&lt;html&gt;&lt;head/&gt;&lt;body&gt;&lt;p&gt;档协议类型支持时，预选匹配的组、认证域(Realm)、域或网关选择项。&lt;/p&gt;&lt;/body&gt;&lt;/html&gt;</translation>
    </message>
    <message>
        <source>Auth Group</source>
        <translation>认证组</translation>
    </message>
    <message>
        <source>&lt;html&gt;&lt;head/&gt;&lt;body&gt;&lt;p&gt;Path part of the server URL, appended to the address above. Load-bearing on gp, f5 and fortinet portals.&lt;/p&gt;&lt;/body&gt;&lt;/html&gt;</source>
        <translation>&lt;html&gt;&lt;head/&gt;&lt;body&gt;&lt;p&gt;服务器 URL 的路径部分，追加到上方的地址之后。在 gp、f5 和 fortinet 门户中为必要项(Load-bearing)。&lt;/p&gt;&lt;/body&gt;&lt;/html&gt;</translation>
    </message>
    <message>
        <source>Server Path</source>
        <translation>服务器路径</translation>
    </message>
    <message>
        <source>&lt;html&gt;&lt;head/&gt;&lt;body&gt;&lt;p&gt;Preferred tunnel MTU. Values below 576 are treated as 576; the maximum is 65535.&lt;/p&gt;&lt;/body&gt;&lt;/html&gt;</source>
        <translation>&lt;html&gt;&lt;head/&gt;&lt;body&gt;&lt;p&gt;首选隧道 MTU。低于 576 的值按 576 处理；最大值为 65535。&lt;/p&gt;&lt;/body&gt;&lt;/html&gt;</translation>
    </message>
    <message>
        <source>MTU</source>
        <translation>MTU</translation>
    </message>
    <message>
        <source>OTP</source>
        <translation>OTP</translation>
    </message>
    <message>
        <source>&lt;html&gt;&lt;head/&gt;&lt;body&gt;&lt;p&gt;Traffic that falls through to this profile reaches the tunnel only when the destination is inside a subnet the VPN server advertises; anything else is blocked. Routing rules that name this profile explicitly are not affected and always win.&lt;/p&gt;&lt;/body&gt;&lt;/html&gt;</source>
        <translation>&lt;html&gt;&lt;head/&gt;&lt;body&gt;&lt;p&gt;落入这个配置档的兜底(falls through)流量，仅当目标地址位于 VPN 服务器广告的子网内时才会进入该隧道；除此之外的任何流量将被拦截。而那些显式指定这个配置档的路由规则不受影响，且始终赢得优先级。&lt;/p&gt;&lt;/body&gt;&lt;/html&gt;</translation>
    </message>
    <message>
        <source>Only route advertised network</source>
        <translation>仅路由通告的网络</translation>
    </message>
    <message>
        <source>&lt;html&gt;&lt;head/&gt;&lt;body&gt;&lt;p&gt;How DNS uses the resolvers the VPN server pushes when this profile is started.&lt;/p&gt;&lt;p&gt;None: ignore them. Use this if the server claims every domain and breaks public DNS.&lt;br/&gt;Prefer: resolve the names the server claims - its split-DNS suffixes and search domains - through them, and everything else as usual.&lt;br/&gt;Strict: also send every remote DNS query to them instead of the remote DNS server, and fail the lookups they cannot answer. Direct lookups are unchanged.&lt;/p&gt;&lt;p&gt;As an endpoint in a routing profile, Prefer and Strict both resolve only the names the server claims.&lt;/p&gt;&lt;/body&gt;&lt;/html&gt;</source>
        <translation>&lt;html&gt;&lt;head/&gt;&lt;body&gt;&lt;p&gt;当启动这个配置档时，DNS 如何使用 VPN 服务器推送的解析器。&lt;/p&gt;&lt;p&gt;None：无，忽略它们。如果服务器声明了所有域名并导致公共 DNS 失效则使用这个选项。&lt;br/&gt;Prefer：优先，仅通过它们（解析器）解析服务器声明的名称（即其分流 DNS 后缀和搜索域），而其余的名称则照常解析。&lt;br/&gt;Strict：严格，也发送每个远程 DNS 查询到解析器（而不是远程 DNS 服务器），若它们无法应答则使解析失败。直接查询不受影响。&lt;/p&gt;&lt;p&gt;作为路由配置档中的端点的情况下，“优先”和“严格”均仅解析服务器声明的名称。&lt;/p&gt;&lt;/body&gt;&lt;/html&gt;</translation>
    </message>
    <message>
        <source>Tunnel DNS</source>
        <translation>隧道 DNS</translation>
    </message>
    <message>
        <source>Advanced</source>
        <translation>高级</translation>
    </message>
    <message>
        <source>TLS</source>
        <translation>TLS</translation>
    </message>
    <message>
        <source>&lt;html&gt;&lt;head/&gt;&lt;body&gt;&lt;p&gt;Disables verification of the server certificate and hostname. This lets an active attacker impersonate the VPN server; prefer a CA certificate or a peer fingerprint.&lt;/p&gt;&lt;/body&gt;&lt;/html&gt;</source>
        <translation>&lt;html&gt;&lt;head/&gt;&lt;body&gt;&lt;p&gt;禁用服务器证书和主机名验证。这会使主动攻击者能够冒充 VPN 服务器；建议优先使用 CA 证书或对端指纹。&lt;/p&gt;&lt;/body&gt;&lt;/html&gt;</translation>
    </message>
    <message>
        <source>Allow insecure</source>
        <translation>允许不安全</translation>
    </message>
    <message>
        <source>&lt;html&gt;&lt;head/&gt;&lt;body&gt;&lt;p&gt;Server name used for TLS SNI and certificate hostname verification. The hostname from the address is used when empty.&lt;/p&gt;&lt;/body&gt;&lt;/html&gt;</source>
        <translation>&lt;html&gt;&lt;head/&gt;&lt;body&gt;&lt;p&gt;用于 TLS SNI 和证书主机名验证的服务器名称。留空时使用来自地址中的主机名。&lt;/p&gt;&lt;/body&gt;&lt;/html&gt;</translation>
    </message>
    <message>
        <source>Server Name</source>
        <translation>服务器名称</translation>
    </message>
    <message>
        <source>&lt;html&gt;&lt;head/&gt;&lt;body&gt;&lt;p&gt;Additional trusted CA certificate content in PEM format, added to the system certificate pool.&lt;/p&gt;&lt;/body&gt;&lt;/html&gt;</source>
        <translation>&lt;html&gt;&lt;head/&gt;&lt;body&gt;&lt;p&gt;采用 PEM 格式的其他受信任 CA （证书颁发机构）证书，添加到系统证书池。&lt;/p&gt;&lt;/body&gt;&lt;/html&gt;</translation>
    </message>
    <message>
        <source>CA Certificate</source>
        <translation>CA 证书</translation>
    </message>
    <message>
        <source>Not set</source>
        <translation>未设置</translation>
    </message>
    <message>
        <source>Client Certificate</source>
        <translation>客户端证书</translation>
    </message>
    <message>
        <source>Client Key</source>
        <translation>客户端钥匙</translation>
    </message>
    <message>
        <source>&lt;html&gt;&lt;head/&gt;&lt;body&gt;&lt;p&gt;Password for the encrypted client private key.&lt;/p&gt;&lt;/body&gt;&lt;/html&gt;</source>
        <translation>&lt;html&gt;&lt;head/&gt;&lt;body&gt;&lt;p&gt;加密客户端私钥的密码。&lt;/p&gt;&lt;/body&gt;&lt;/html&gt;</translation>
    </message>
    <message>
        <source>Client Key Password</source>
        <translation>客户端钥匙密码</translation>
    </message>
    <message>
        <source>Prefer</source>
        <translation>优先</translation>
    </message>
    <message>
        <source>Strict</source>
        <translation>严格</translation>
    </message>
    <message>
        <source>&lt;html&gt;&lt;head/&gt;&lt;body&gt;&lt;p&gt;Binds an authenticator entry to this profile. %1 in the username, password, software token or form entry fields is replaced with a generated code at connect time.&lt;/p&gt;&lt;/body&gt;&lt;/html&gt;</source>
        <translation>&lt;html&gt;&lt;head/&gt;&lt;body&gt;&lt;p&gt;绑定身份验证器条目到这个配置档。用户名、密码、软件令牌或表单条目字段中的 %1 会在连接时被生成的代码替换。&lt;/p&gt;&lt;/body&gt;&lt;/html&gt;</translation>
    </message>
    <message>
        <source>%1 is replaced at connect time</source>
        <translation>%1 在连接时被替换</translation>
    </message>
    <message>
        <source>Already set</source>
        <translation>已设置</translation>
    </message>
    <message>
        <source>This protocol does not accept the reported OS &quot;%1&quot; set under Advanced.</source>
        <translation>这个协议不接受在「高级」下设置的报告操作系统 &quot;%1&quot;。</translation>
    </message>
    <message>
        <source>The token or form entries under Advanced use %1; select an authenticator entry.</source>
        <translation>「高级」选项下的令牌或表单条目使用 %1；请选择一个身份验证条目。</translation>
    </message>
    <message>
        <source>None</source>
        <translation>无</translation>
    </message>
</context>
<context>
    <name>EditOpenConnectAdvanced</name>
    <message>
        <source>OpenConnect Advanced</source>
        <translation>OpenConnect 高级</translation>
    </message>
    <message>
        <source>Session</source>
        <translation>会话</translation>
    </message>
    <message>
        <source>&lt;html&gt;&lt;head/&gt;&lt;body&gt;&lt;p&gt;Existing authentication session used to connect without first prompting for credentials. The accepted format depends on the flavor.&lt;/p&gt;&lt;/body&gt;&lt;/html&gt;</source>
        <translation>&lt;html&gt;&lt;head/&gt;&lt;body&gt;&lt;p&gt;使用现有认证会话连接，而无需先提示输入凭据。可接受的格式取决于协议类型。&lt;/p&gt;&lt;/body&gt;&lt;/html&gt;</translation>
    </message>
    <message>
        <source>Cookie</source>
        <translation>Cookie</translation>
    </message>
    <message>
        <source>&lt;html&gt;&lt;head/&gt;&lt;body&gt;&lt;p&gt;Operating system identity reported to the server. anyconnect, gp and pulse accept linux, linux-64, win, mac-intel, android and apple-ios. The system platform is used by default.&lt;/p&gt;&lt;/body&gt;&lt;/html&gt;</source>
        <translation>&lt;html&gt;&lt;head/&gt;&lt;body&gt;&lt;p&gt;向服务器报告的操作系统标识。anyconnect, gp 和 pulse 接受 linux, linux-64, win, mac-intel, android 和 apple-ios。默认使用系统平台。&lt;/p&gt;&lt;/body&gt;&lt;/html&gt;</translation>
    </message>
    <message>
        <source>Reported OS</source>
        <translation>报告的OS</translation>
    </message>
    <message>
        <source>&lt;html&gt;&lt;head/&gt;&lt;body&gt;&lt;p&gt;User agent reported to the server. The default is flavor specific.&lt;/p&gt;&lt;/body&gt;&lt;/html&gt;</source>
        <translation>&lt;html&gt;&lt;head/&gt;&lt;body&gt;&lt;p&gt;报告给服务器的“用户代理”(User agent)。默认值因协议类型而定。&lt;/p&gt;&lt;/body&gt;&lt;/html&gt;</translation>
    </message>
    <message>
        <source>User Agent</source>
        <translation>用户代理</translation>
    </message>
    <message>
        <source>&lt;html&gt;&lt;head/&gt;&lt;body&gt;&lt;p&gt;Client version reported separately from the user agent. Currently used by AnyConnect XML authentication.&lt;/p&gt;&lt;/body&gt;&lt;/html&gt;</source>
        <translation>&lt;html&gt;&lt;head/&gt;&lt;body&gt;&lt;p&gt;客户端版本与用户代理分开报告。目前由 AnyConnect XML 身份验证使用。&lt;/p&gt;&lt;/body&gt;&lt;/html&gt;</translation>
    </message>
    <message>
        <source>Version</source>
        <translation>版本</translation>
    </message>
    <message>
        <source>v9.21 (default)</source>
        <translation>v9.21 (默认)</translation>
    </message>
    <message>
        <source>&lt;html&gt;&lt;head/&gt;&lt;body&gt;&lt;p&gt;Local hostname reported to the server. The system hostname is used by default.&lt;/p&gt;&lt;/body&gt;&lt;/html&gt;</source>
        <translation>&lt;html&gt;&lt;head/&gt;&lt;body&gt;&lt;p&gt;报告给服务器的本地主机名。默认使用系统主机名。&lt;/p&gt;&lt;/body&gt;&lt;/html&gt;</translation>
    </message>
    <message>
        <source>Local Hostname</source>
        <translation>本地主机名</translation>
    </message>
    <message>
        <source>&lt;html&gt;&lt;head/&gt;&lt;body&gt;&lt;p&gt;This is the token the core generates itself. To use a Throne OTP profile, bind it on the main editor instead.&lt;/p&gt;&lt;/body&gt;&lt;/html&gt;</source>
        <translation>&lt;html&gt;&lt;head/&gt;&lt;body&gt;&lt;p&gt;这是核心自己生成的令牌。要使用 Throne OTP 配置档，请改为在主编辑器上绑定它。&lt;/p&gt;&lt;/body&gt;&lt;/html&gt;</translation>
    </message>
    <message>
        <source>Software Token</source>
        <translation>软件令牌</translation>
    </message>
    <message>
        <source>&lt;html&gt;&lt;head/&gt;&lt;body&gt;&lt;p&gt;totp and hotp are one-time password modes, stoken is an RSA SecurID software token and oidc is an access token used for HTTP Bearer authentication.&lt;/p&gt;&lt;/body&gt;&lt;/html&gt;</source>
        <translation>&lt;html&gt;&lt;head/&gt;&lt;body&gt;&lt;p&gt;totp 和 hotp 是一次性密码模式，stoken 是一个 RSA SecurID 软件令牌，oidc 是用于 HTTP Bearer 认证的访问令牌。&lt;/p&gt;&lt;/body&gt;&lt;/html&gt;</translation>
    </message>
    <message>
        <source>Mode</source>
        <translation>模式</translation>
    </message>
    <message>
        <source>&lt;html&gt;&lt;head/&gt;&lt;body&gt;&lt;p&gt;A Base32 secret, a base32: prefixed secret or an otpauth:// URI for totp and hotp; the encoded CTF content for stoken; the access token for oidc. Conflicts with the secret path.&lt;/p&gt;&lt;/body&gt;&lt;/html&gt;</source>
        <translation>&lt;html&gt;&lt;head/&gt;&lt;body&gt;&lt;p&gt;一个 Base32 密钥，一个前缀 base32: 的密钥或用于 TOTP 和 HOTP 的 otpauth://URI；用于 stoken 的编码 CTF 内容；用于 OIDC 的访问令牌。与密码路径不能同时使用。&lt;/p&gt;&lt;/body&gt;&lt;/html&gt;</translation>
    </message>
    <message>
        <source>Secret</source>
        <translation>密钥</translation>
    </message>
    <message>
        <source>&lt;html&gt;&lt;head/&gt;&lt;body&gt;&lt;p&gt;Path to the token secret or access token. Conflicts with the inline secret.&lt;/p&gt;&lt;/body&gt;&lt;/html&gt;</source>
        <translation>&lt;html&gt;&lt;head/&gt;&lt;body&gt;&lt;p&gt;令牌密钥或访问令牌的路径。与内联密钥冲突（不能同时使用）。&lt;/p&gt;&lt;/body&gt;&lt;/html&gt;</translation>
    </message>
    <message>
        <source>Secret Path</source>
        <translation>密钥路径</translation>
    </message>
    <message>
        <source>&lt;html&gt;&lt;head/&gt;&lt;body&gt;&lt;p&gt;RSA SecurID PIN for stoken mode.&lt;/p&gt;&lt;/body&gt;&lt;/html&gt;</source>
        <translation>&lt;html&gt;&lt;head/&gt;&lt;body&gt;&lt;p&gt;用于 stoken 模式的 RSA SecurID PIN。&lt;/p&gt;&lt;/body&gt;&lt;/html&gt;</translation>
    </message>
    <message>
        <source>PIN</source>
        <translation>PIN</translation>
    </message>
    <message>
        <source>&lt;html&gt;&lt;head/&gt;&lt;body&gt;&lt;p&gt;Password for decrypting a password-protected RSA SecurID token.&lt;/p&gt;&lt;/body&gt;&lt;/html&gt;</source>
        <translation>&lt;html&gt;&lt;head/&gt;&lt;body&gt;&lt;p&gt;用于解密受密码保护的 RSA SecurID 令牌的密码。&lt;/p&gt;&lt;/body&gt;&lt;/html&gt;</translation>
    </message>
    <message>
        <source>Password</source>
        <translation>密码</translation>
    </message>
    <message>
        <source>&lt;html&gt;&lt;head/&gt;&lt;body&gt;&lt;p&gt;Device ID for decrypting a device-bound RSA SecurID token.&lt;/p&gt;&lt;/body&gt;&lt;/html&gt;</source>
        <translation>&lt;html&gt;&lt;head/&gt;&lt;body&gt;&lt;p&gt;用于解密设备绑定 RSA SecurID 令牌的设备 ID。&lt;/p&gt;&lt;/body&gt;&lt;/html&gt;</translation>
    </message>
    <message>
        <source>Device ID</source>
        <translation>设备 ID</translation>
    </message>
    <message>
        <source>&lt;html&gt;&lt;head/&gt;&lt;body&gt;&lt;p&gt;Initial counter for hotp mode. Empty uses the counter from an otpauth:// URI, otherwise zero.&lt;/p&gt;&lt;/body&gt;&lt;/html&gt;</source>
        <translation>&lt;html&gt;&lt;head/&gt;&lt;body&gt;&lt;p&gt;HOTP 模式的初始计数器。为空时使用来自 otpauth:// URI 中的计数器，否则为零。&lt;/p&gt;&lt;/body&gt;&lt;/html&gt;</translation>
    </message>
    <message>
        <source>Counter</source>
        <translation>计数器</translation>
    </message>
    <message>
        <source>TLS</source>
        <translation>TLS</translation>
    </message>
    <message>
        <source>&lt;html&gt;&lt;head/&gt;&lt;body&gt;&lt;p&gt;Allowed server certificate fingerprints, one per line. Accepts a bare SHA-1 fingerprint or sha1:, sha256: and pin-sha256: values, abbreviated to at least four characters.&lt;/p&gt;&lt;/body&gt;&lt;/html&gt;</source>
        <translation>&lt;html&gt;&lt;head/&gt;&lt;body&gt;&lt;p&gt;允许的服务器证书指纹，每行一个。可接受裸 SHA-1 指纹或 sha1:、sha256: 及 pin-sha256: 值，缩写为至少四个字符。&lt;/p&gt;&lt;/body&gt;&lt;/html&gt;</translation>
    </message>
    <message>
        <source>Peer Fingerprint</source>
        <translation>对端指纹</translation>
    </message>
    <message>
        <source>Not set</source>
        <translation>未设置</translation>
    </message>
    <message>
        <source>&lt;html&gt;&lt;head/&gt;&lt;body&gt;&lt;p&gt;Use a CA certificate or a peer fingerprint to establish trust when this is enabled.&lt;/p&gt;&lt;/body&gt;&lt;/html&gt;</source>
        <translation>&lt;html&gt;&lt;head/&gt;&lt;body&gt;&lt;p&gt;启用这个功能时，使用 CA 证书或对等方指纹来建立信任。&lt;/p&gt;&lt;/body&gt;&lt;/html&gt;</translation>
    </message>
    <message>
        <source>Disable the system CA certificate pool</source>
        <translation>禁用系统的 CA 证书池</translation>
    </message>
    <message>
        <source>&lt;html&gt;&lt;head/&gt;&lt;body&gt;&lt;p&gt;Conflicts with the inline CA certificate on the main editor.&lt;/p&gt;&lt;/body&gt;&lt;/html&gt;</source>
        <translation>&lt;html&gt;&lt;head/&gt;&lt;body&gt;&lt;p&gt;与主编辑器上的内联 CA 证书冲突（不能同时使用）。&lt;/p&gt;&lt;/body&gt;&lt;/html&gt;</translation>
    </message>
    <message>
        <source>CA Certificate Path</source>
        <translation>CA 证书路径</translation>
    </message>
    <message>
        <source>&lt;html&gt;&lt;head/&gt;&lt;body&gt;&lt;p&gt;Conflicts with the inline client certificate on the main editor.&lt;/p&gt;&lt;/body&gt;&lt;/html&gt;</source>
        <translation>&lt;html&gt;&lt;head/&gt;&lt;body&gt;&lt;p&gt;与主编辑器上的内联客户端证书冲突（不能同时使用）。&lt;/p&gt;&lt;/body&gt;&lt;/html&gt;</translation>
    </message>
    <message>
        <source>Client Certificate Path</source>
        <translation>客户端证书路径</translation>
    </message>
    <message>
        <source>&lt;html&gt;&lt;head/&gt;&lt;body&gt;&lt;p&gt;Conflicts with the inline client key on the main editor.&lt;/p&gt;&lt;/body&gt;&lt;/html&gt;</source>
        <translation>&lt;html&gt;&lt;head/&gt;&lt;body&gt;&lt;p&gt;与主编辑器上的内联客户端密钥冲突（不能同时使用）。&lt;/p&gt;&lt;/body&gt;&lt;/html&gt;</translation>
    </message>
    <message>
        <source>Client Key Path</source>
        <translation>客户端钥匙路径</translation>
    </message>
    <message>
        <source>&lt;html&gt;&lt;head/&gt;&lt;body&gt;&lt;p&gt;AnyConnect multiple-certificate authentication chain in PEM format. Conflicts with the MCA certificate path.&lt;/p&gt;&lt;/body&gt;&lt;/html&gt;</source>
        <translation>&lt;html&gt;&lt;head/&gt;&lt;body&gt;&lt;p&gt;AnyConnect 多证书认证链，采用 PEM 格式。与 MCA 证书路径不能同时使用。&lt;/p&gt;&lt;/body&gt;&lt;/html&gt;</translation>
    </message>
    <message>
        <source>MCA Certificate</source>
        <translation>MCA 证书</translation>
    </message>
    <message>
        <source>&lt;html&gt;&lt;head/&gt;&lt;body&gt;&lt;p&gt;Conflicts with the inline MCA certificate.&lt;/p&gt;&lt;/body&gt;&lt;/html&gt;</source>
        <translation>&lt;html&gt;&lt;head/&gt;&lt;body&gt;&lt;p&gt;与内联 MCA 证书冲突（不能同时使用）。&lt;/p&gt;&lt;/body&gt;&lt;/html&gt;</translation>
    </message>
    <message>
        <source>MCA Certificate Path</source>
        <translation>MCA 证书路径</translation>
    </message>
    <message>
        <source>&lt;html&gt;&lt;head/&gt;&lt;body&gt;&lt;p&gt;AnyConnect multiple-certificate authentication private key in PEM format. Conflicts with the MCA key path.&lt;/p&gt;&lt;/body&gt;&lt;/html&gt;</source>
        <translation>&lt;html&gt;&lt;head/&gt;&lt;body&gt;&lt;p&gt;AnyConnect 多证书认证私钥使用 PEM 格式。与 MCA 密钥路径冲突（不能同时使用）。&lt;/p&gt;&lt;/body&gt;&lt;/html&gt;</translation>
    </message>
    <message>
        <source>MCA Key</source>
        <translation>MCA 钥匙</translation>
    </message>
    <message>
        <source>&lt;html&gt;&lt;head/&gt;&lt;body&gt;&lt;p&gt;Conflicts with the inline MCA key.&lt;/p&gt;&lt;/body&gt;&lt;/html&gt;</source>
        <translation>&lt;html&gt;&lt;head/&gt;&lt;body&gt;&lt;p&gt;与内联 MCA 键冲突（不能同时使用）。&lt;/p&gt;&lt;/body&gt;&lt;/html&gt;</translation>
    </message>
    <message>
        <source>MCA Key Path</source>
        <translation>MCA 钥匙路径</translation>
    </message>
    <message>
        <source>&lt;html&gt;&lt;head/&gt;&lt;body&gt;&lt;p&gt;Password for the encrypted MCA private key.&lt;/p&gt;&lt;/body&gt;&lt;/html&gt;</source>
        <translation>&lt;html&gt;&lt;head/&gt;&lt;body&gt;&lt;p&gt;加密的 MCA 私钥的密码。&lt;/p&gt;&lt;/body&gt;&lt;/html&gt;</translation>
    </message>
    <message>
        <source>MCA Key Password</source>
        <translation>MCA 钥匙密码</translation>
    </message>
    <message>
        <source>&lt;html&gt;&lt;head/&gt;&lt;body&gt;&lt;p&gt;All three fields are required when a mobile identity is configured.&lt;/p&gt;&lt;/body&gt;&lt;/html&gt;</source>
        <translation>&lt;html&gt;&lt;head/&gt;&lt;body&gt;&lt;p&gt;当配置移动身份时，所有三个字段都是必需的。&lt;/p&gt;&lt;/body&gt;&lt;/html&gt;</translation>
    </message>
    <message>
        <source>Mobile Identity</source>
        <translation>移动身份</translation>
    </message>
    <message>
        <source>&lt;html&gt;&lt;head/&gt;&lt;body&gt;&lt;p&gt;Mobile operating system version reported to the AnyConnect server.&lt;/p&gt;&lt;/body&gt;&lt;/html&gt;</source>
        <translation>&lt;html&gt;&lt;head/&gt;&lt;body&gt;&lt;p&gt;向 AnyConnect 服务器报告的移动操作系统版本。&lt;/p&gt;&lt;/body&gt;&lt;/html&gt;</translation>
    </message>
    <message>
        <source>Platform Version</source>
        <translation>平台版本</translation>
    </message>
    <message>
        <source>&lt;html&gt;&lt;head/&gt;&lt;body&gt;&lt;p&gt;Mobile device model or type reported to the AnyConnect server.&lt;/p&gt;&lt;/body&gt;&lt;/html&gt;</source>
        <translation>&lt;html&gt;&lt;head/&gt;&lt;body&gt;&lt;p&gt;向 AnyConnect 服务器报告的移动设备型号或类型。&lt;/p&gt;&lt;/body&gt;&lt;/html&gt;</translation>
    </message>
    <message>
        <source>Device Type</source>
        <translation>设备类型</translation>
    </message>
    <message>
        <source>&lt;html&gt;&lt;head/&gt;&lt;body&gt;&lt;p&gt;Mobile device identifier reported to the AnyConnect server.&lt;/p&gt;&lt;/body&gt;&lt;/html&gt;</source>
        <translation>&lt;html&gt;&lt;head/&gt;&lt;body&gt;&lt;p&gt;向 AnyConnect 服务器报告的移动设备标识符。&lt;/p&gt;&lt;/body&gt;&lt;/html&gt;</translation>
    </message>
    <message>
        <source>Device Unique ID</source>
        <translation>设备唯一 ID</translation>
    </message>
    <message>
        <source>Compliance</source>
        <translation>合规检查</translation>
    </message>
    <message>
        <source>&lt;html&gt;&lt;head/&gt;&lt;body&gt;&lt;p&gt;External AnyConnect CSD / host scan wrapper executable. Built-in handling is used when empty.&lt;/p&gt;&lt;/body&gt;&lt;/html&gt;</source>
        <translation>&lt;html&gt;&lt;head/&gt;&lt;body&gt;&lt;p&gt;外部 AnyConnect CSD / 主机扫描封装可执行文件。为空时使用内置处理。&lt;/p&gt;&lt;/body&gt;&lt;/html&gt;</translation>
    </message>
    <message>
        <source>CSD Wrapper Path</source>
        <translation>CSD 封装器路径</translation>
    </message>
    <message>
        <source>&lt;html&gt;&lt;head/&gt;&lt;body&gt;&lt;p&gt;External GlobalProtect HIP report wrapper executable. Built-in reporting is used when empty.&lt;/p&gt;&lt;/body&gt;&lt;/html&gt;</source>
        <translation>&lt;html&gt;&lt;head/&gt;&lt;body&gt;&lt;p&gt;外部 GlobalProtect HIP 报告封装可执行文件。当为空时使用内置报告。&lt;/p&gt;&lt;/body&gt;&lt;/html&gt;</translation>
    </message>
    <message>
        <source>HIP Wrapper Path</source>
        <translation>HIP 封装器路径</translation>
    </message>
    <message>
        <source>&lt;html&gt;&lt;head/&gt;&lt;body&gt;&lt;p&gt;Fortinet hostcheck result string, conventionally security-status,os-version such as 0100,10.0.19042. An empty value disables hostcheck.&lt;/p&gt;&lt;/body&gt;&lt;/html&gt;</source>
        <translation>&lt;html&gt;&lt;head/&gt;&lt;body&gt;&lt;p&gt;Fortinet 主机检查结果字符串，通常为 security-status,os-version，例如 0100,10.0.19042。空值将禁用主机检查。&lt;/p&gt;&lt;/body&gt;&lt;/html&gt;</translation>
    </message>
    <message>
        <source>Fortinet Hostcheck</source>
        <translation>Fortinet 主机检查</translation>
    </message>
    <message>
        <source>&lt;html&gt;&lt;head/&gt;&lt;body&gt;&lt;p&gt;Fortinet virtual desktop check result, conventionally colon separated MAC addresses joined by a vertical bar.&lt;/p&gt;&lt;/body&gt;&lt;/html&gt;</source>
        <translation>&lt;html&gt;&lt;head/&gt;&lt;body&gt;&lt;p&gt;Fortinet 虚拟桌面检查结果，通常是冒号间隔的用竖线串连的 MAC 地址。&lt;/p&gt;&lt;/body&gt;&lt;/html&gt;</translation>
    </message>
    <message>
        <source>Fortinet Virtual Desktop</source>
        <translation>Fortinet 虚拟桌面</translation>
    </message>
    <message>
        <source>TNCC</source>
        <translation>TNCC</translation>
    </message>
    <message>
        <source>&lt;html&gt;&lt;head/&gt;&lt;body&gt;&lt;p&gt;External Network Connect TNCC wrapper executable. Conflicts with every other field in this group.&lt;/p&gt;&lt;/body&gt;&lt;/html&gt;</source>
        <translation>&lt;html&gt;&lt;head/&gt;&lt;body&gt;&lt;p&gt;外部网络连接 TNCC 封装器可执行文件。与这个组中的每个其他字段冲突（不能同时使用）。&lt;/p&gt;&lt;/body&gt;&lt;/html&gt;</translation>
    </message>
    <message>
        <source>Wrapper Path</source>
        <translation>封装器路径</translation>
    </message>
    <message>
        <source>&lt;html&gt;&lt;head/&gt;&lt;body&gt;&lt;p&gt;Device ID reported by the built-in TNCC handler.&lt;/p&gt;&lt;/body&gt;&lt;/html&gt;</source>
        <translation>&lt;html&gt;&lt;head/&gt;&lt;body&gt;&lt;p&gt;由内置 TNCC 处理程序报告的设备 ID。&lt;/p&gt;&lt;/body&gt;&lt;/html&gt;</translation>
    </message>
    <message>
        <source>&lt;html&gt;&lt;head/&gt;&lt;body&gt;&lt;p&gt;User agent used by the built-in TNCC handler. Neoteris HC Http is used by default.&lt;/p&gt;&lt;/body&gt;&lt;/html&gt;</source>
        <translation>&lt;html&gt;&lt;head/&gt;&lt;body&gt;&lt;p&gt;内置 TNCC 处理程序使用的用户代理。默认使用 Neoteris HC Http。&lt;/p&gt;&lt;/body&gt;&lt;/html&gt;</translation>
    </message>
    <message>
        <source>&lt;html&gt;&lt;head/&gt;&lt;body&gt;&lt;p&gt;Reports the platform, hostname and observed MAC addresses. Required before machine certificates are used.&lt;/p&gt;&lt;/body&gt;&lt;/html&gt;</source>
        <translation>&lt;html&gt;&lt;head/&gt;&lt;body&gt;&lt;p&gt;报告平台、主机名和观察到的 MAC 地址。在使用机器证书前所需。&lt;/p&gt;&lt;/body&gt;&lt;/html&gt;</translation>
    </message>
    <message>
        <source>Enable machine identification</source>
        <translation>启用机器识别</translation>
    </message>
    <message>
        <source>&lt;html&gt;&lt;head/&gt;&lt;body&gt;&lt;p&gt;Machine certificates used by the built-in TNCC handler to answer certificate requests. Requires machine identification.&lt;/p&gt;&lt;/body&gt;&lt;/html&gt;</source>
        <translation>&lt;html&gt;&lt;head/&gt;&lt;body&gt;&lt;p&gt;内置 TNCC 处理程序用来响应证书请求的机器证书。需要机器身份识别。&lt;/p&gt;&lt;/body&gt;&lt;/html&gt;</translation>
    </message>
    <message>
        <source>Certificate</source>
        <translation>证书</translation>
    </message>
    <message>
        <source>Certificate Path</source>
        <translation>证书路径</translation>
    </message>
    <message>
        <source>Add</source>
        <translation>添加</translation>
    </message>
    <message>
        <source>Remove</source>
        <translation>删除</translation>
    </message>
    <message>
        <source>Edit Certificate</source>
        <translation>编辑证书</translation>
    </message>
    <message>
        <source>Authentication Forms</source>
        <translation>认证表单</translation>
    </message>
    <message>
        <source>&lt;html&gt;&lt;head/&gt;&lt;body&gt;&lt;p&gt;An entry matches by submission key when set, or by the combination of form ID and name. Later matching entries take precedence. Ask interactively conflicts with a value.&lt;/p&gt;&lt;/body&gt;&lt;/html&gt;</source>
        <translation>&lt;html&gt;&lt;head/&gt;&lt;body&gt;&lt;p&gt;当设置了提交键时，条目会按提交键匹配，或者按表单 ID 和名称的组合匹配。靠后的匹配条目优先。交互式询问会与某个值冲突。&lt;/p&gt;&lt;/body&gt;&lt;/html&gt;</translation>
    </message>
    <message>
        <source>Form ID</source>
        <translation>表单 ID</translation>
    </message>
    <message>
        <source>Submission Key</source>
        <translation>提交键</translation>
    </message>
    <message>
        <source>Name</source>
        <translation>名称</translation>
    </message>
    <message>
        <source>Value</source>
        <translation>值</translation>
    </message>
    <message>
        <source>Ask interactively</source>
        <translation>交互式提问</translation>
    </message>
    <message>
        <source>Transport</source>
        <translation>传输</translation>
    </message>
    <message>
        <source>&lt;html&gt;&lt;head/&gt;&lt;body&gt;&lt;p&gt;Uses the TLS data channel only.&lt;/p&gt;&lt;/body&gt;&lt;/html&gt;</source>
        <translation>&lt;html&gt;&lt;head/&gt;&lt;body&gt;&lt;p&gt;仅使用 TLS 数据通道。&lt;/p&gt;&lt;/body&gt;&lt;/html&gt;</translation>
    </message>
    <message>
        <source>Disable the DTLS or ESP data channel</source>
        <translation>禁用 DTLS 或 ESP 数据通道</translation>
    </message>
    <message>
        <source>&lt;html&gt;&lt;head/&gt;&lt;body&gt;&lt;p&gt;Local UDP port used by the secondary data channel. An ephemeral port is selected by default.&lt;/p&gt;&lt;/body&gt;&lt;/html&gt;</source>
        <translation>&lt;html&gt;&lt;head/&gt;&lt;body&gt;&lt;p&gt;次要数据通道使用的本地 UDP 端口。默认情况下会选择一个临时(ephemeral)端口。&lt;/p&gt;&lt;/body&gt;&lt;/html&gt;</translation>
    </message>
    <message>
        <source>DTLS Local Port</source>
        <translation>DTLS 本地端口</translation>
    </message>
    <message>
        <source>&lt;html&gt;&lt;head/&gt;&lt;body&gt;&lt;p&gt;Compression can weaken traffic confidentiality. Conflicts with the compression mode all.&lt;/p&gt;&lt;/body&gt;&lt;/html&gt;</source>
        <translation>&lt;html&gt;&lt;head/&gt;&lt;body&gt;&lt;p&gt;压缩可能削弱流量的保密性。与压缩模式 all (全部)冲突（不能同时使用）。&lt;/p&gt;&lt;/body&gt;&lt;/html&gt;</translation>
    </message>
    <message>
        <source>Disable compression negotiation</source>
        <translation>禁用压缩协商</translation>
    </message>
    <message>
        <source>&lt;html&gt;&lt;head/&gt;&lt;body&gt;&lt;p&gt;stateless is used by default. all additionally advertises stateful deflate for CSTP and carries extra confidentiality risk.&lt;/p&gt;&lt;/body&gt;&lt;/html&gt;</source>
        <translation>&lt;html&gt;&lt;head/&gt;&lt;body&gt;&lt;p&gt;默认使用 stateless (无状态)。all 额外为 CSTP 宣传有状态 deflate (压缩)，并带来额外的机密性风险。&lt;/p&gt;&lt;/body&gt;&lt;/html&gt;</translation>
    </message>
    <message>
        <source>Compression Mode</source>
        <translation>压缩模式</translation>
    </message>
    <message>
        <source>Disable IPv6 tunnel configuration</source>
        <translation>禁用 IPv6 隧道配置</translation>
    </message>
    <message>
        <source>&lt;html&gt;&lt;head/&gt;&lt;body&gt;&lt;p&gt;Applies to authentication and configuration requests.&lt;/p&gt;&lt;/body&gt;&lt;/html&gt;</source>
        <translation>&lt;html&gt;&lt;head/&gt;&lt;body&gt;&lt;p&gt;适用于身份验证和配置请求。&lt;/p&gt;&lt;/body&gt;&lt;/html&gt;</translation>
    </message>
    <message>
        <source>Disable HTTP connection reuse</source>
        <translation>禁用 HTTP 连接重用</translation>
    </message>
    <message>
        <source>&lt;html&gt;&lt;head/&gt;&lt;body&gt;&lt;p&gt;Starts AnyConnect authentication with the legacy GET flow.&lt;/p&gt;&lt;/body&gt;&lt;/html&gt;</source>
        <translation>&lt;html&gt;&lt;head/&gt;&lt;body&gt;&lt;p&gt;用旧版的 GET 流程启动 AnyConnect 认证。&lt;/p&gt;&lt;/body&gt;&lt;/html&gt;</translation>
    </message>
    <message>
        <source>Disable XML POST authentication</source>
        <translation>禁用 XML POST 认证</translation>
    </message>
    <message>
        <source>&lt;html&gt;&lt;head/&gt;&lt;body&gt;&lt;p&gt;External authentication is not advertised and any unexpected SSO or SAML request is rejected.&lt;/p&gt;&lt;/body&gt;&lt;/html&gt;</source>
        <translation>&lt;html&gt;&lt;head/&gt;&lt;body&gt;&lt;p&gt;不宣告外部认证，并拒绝任何预期外的 SSO (单点登录)或 SAML (安全断言标记语言)请求。&lt;/p&gt;&lt;/body&gt;&lt;/html&gt;</translation>
    </message>
    <message>
        <source>Disable external browser authentication</source>
        <translation>禁用外部浏览器认证</translation>
    </message>
    <message>
        <source>&lt;html&gt;&lt;head/&gt;&lt;body&gt;&lt;p&gt;Matches OpenConnect --no-passwd. Does not affect other flavors or a session supplied by a cookie.&lt;/p&gt;&lt;/body&gt;&lt;/html&gt;</source>
        <translation>&lt;html&gt;&lt;head/&gt;&lt;body&gt;&lt;p&gt;匹配 OpenConnect --no-passwd 选项。对其他协议类型或通过 cookie 提供的会话不影响。&lt;/p&gt;&lt;/body&gt;&lt;/html&gt;</translation>
    </message>
    <message>
        <source>Abort on a non-success authentication form</source>
        <translation>认证表单不成功时中止</translation>
    </message>
    <message>
        <source>&lt;html&gt;&lt;head/&gt;&lt;body&gt;&lt;p&gt;Disabled by default to match OpenConnect.&lt;/p&gt;&lt;/body&gt;&lt;/html&gt;</source>
        <translation>&lt;html&gt;&lt;head/&gt;&lt;body&gt;&lt;p&gt;默认禁用以匹配 OpenConnect。&lt;/p&gt;&lt;/body&gt;&lt;/html&gt;</translation>
    </message>
    <message>
        <source>Enable TCP keep alive</source>
        <translation>启用 TCP 保活</translation>
    </message>
    <message>
        <source>&lt;html&gt;&lt;head/&gt;&lt;body&gt;&lt;p&gt;For TLS 1.2 and earlier. Disabled by default for servers that require RSA key exchange.&lt;/p&gt;&lt;/body&gt;&lt;/html&gt;</source>
        <translation>&lt;html&gt;&lt;head/&gt;&lt;body&gt;&lt;p&gt;适用于 TLS 1.2 及更早版本。对于需要 RSA 密钥交换的服务器，默认禁用。&lt;/p&gt;&lt;/body&gt;&lt;/html&gt;</translation>
    </message>
    <message>
        <source>Require forward-secret cipher suites</source>
        <translation>要求“前向保密”密码套件</translation>
    </message>
    <message>
        <source>&lt;html&gt;&lt;head/&gt;&lt;body&gt;&lt;p&gt;Enables weak cipher suites and TLS 1.0 compatibility. It does not disable server certificate verification.&lt;/p&gt;&lt;/body&gt;&lt;/html&gt;</source>
        <translation>&lt;html&gt;&lt;head/&gt;&lt;body&gt;&lt;p&gt;启用弱密码套件并兼容 TLS 1.0 。它不会禁用服务器证书验证。&lt;/p&gt;&lt;/body&gt;&lt;/html&gt;</translation>
    </message>
    <message>
        <source>Allow legacy TLS and DTLS crypto</source>
        <translation>允许旧版 TLS 和 DTLS 加密算法</translation>
    </message>
    <message>
        <source>&lt;html&gt;&lt;head/&gt;&lt;body&gt;&lt;p&gt;Base path MTU used to calculate the tunnel MTU after outer IP, transport and protocol overhead. Values below 1280 are treated as 1280.&lt;/p&gt;&lt;/body&gt;&lt;/html&gt;</source>
        <translation>&lt;html&gt;&lt;head/&gt;&lt;body&gt;&lt;p&gt;用于在外部 IP、传输和协议开销之后计算隧道 MTU 的基础路径 MTU。低于 1280 的值将被视为 1280。&lt;/p&gt;&lt;/body&gt;&lt;/html&gt;</translation>
    </message>
    <message>
        <source>Base MTU</source>
        <translation>基本 MTU</translation>
    </message>
    <message>
        <source>1406 (default)</source>
        <translation>1406 (默认)</translation>
    </message>
    <message>
        <source>&lt;html&gt;&lt;head/&gt;&lt;body&gt;&lt;p&gt;Overrides the dead peer detection interval. Positive values below 2s are treated as 2s.&lt;/p&gt;&lt;/body&gt;&lt;/html&gt;</source>
        <translation>&lt;html&gt;&lt;head/&gt;&lt;body&gt;&lt;p&gt;覆盖失效对端(dead peer)检测间隔。小于 2 秒的正值将被视为 2 秒。&lt;/p&gt;&lt;/body&gt;&lt;/html&gt;</translation>
    </message>
    <message>
        <source>DPD Interval</source>
        <translation>DPD 间隔</translation>
    </message>
    <message>
        <source>&lt;html&gt;&lt;head/&gt;&lt;body&gt;&lt;p&gt;Maximum accumulated backoff time after failed reconnect attempts.&lt;/p&gt;&lt;/body&gt;&lt;/html&gt;</source>
        <translation>&lt;html&gt;&lt;head/&gt;&lt;body&gt;&lt;p&gt;重新尝试连接失败后的最大累计退避时间。&lt;/p&gt;&lt;/body&gt;&lt;/html&gt;</translation>
    </message>
    <message>
        <source>Reconnect Timeout</source>
        <translation>重新连接超时</translation>
    </message>
    <message>
        <source>300s (default)</source>
        <translation>300s (300秒，默认)</translation>
    </message>
    <message>
        <source>&lt;html&gt;&lt;head/&gt;&lt;body&gt;&lt;p&gt;Overrides the interval between GlobalProtect HIP reports or Network Connect TNCC checks.&lt;/p&gt;&lt;/body&gt;&lt;/html&gt;</source>
        <translation>&lt;html&gt;&lt;head/&gt;&lt;body&gt;&lt;p&gt;覆盖 GlobalProtect HIP 报告或 Network Connect TNCC 检查之间的间隔。&lt;/p&gt;&lt;/body&gt;&lt;/html&gt;</translation>
    </message>
    <message>
        <source>Trojan Interval</source>
        <translation>Trojan 间隔</translation>
    </message>
    <message>
        <source>&lt;html&gt;&lt;head/&gt;&lt;body&gt;&lt;p&gt;Inbound and outbound packet queue length between the VPN transport and the tunnel interface.&lt;/p&gt;&lt;/body&gt;&lt;/html&gt;</source>
        <translation>&lt;html&gt;&lt;head/&gt;&lt;body&gt;&lt;p&gt;VPN 传输与隧道接口之间的入站和出站(收发)数据包队列长度。&lt;/p&gt;&lt;/body&gt;&lt;/html&gt;</translation>
    </message>
    <message>
        <source>Queue Length</source>
        <translation>队列长度</translation>
    </message>
    <message>
        <source>32 (default)</source>
        <translation>32 (默认)</translation>
    </message>
    <message>
        <source>Already set</source>
        <translation>已设置</translation>
    </message>
    <message>
        <source>TNCC Machine Certificate</source>
        <translation>TNCC 机器证书</translation>
    </message>
    <message>
        <source>Row %1: %2</source>
        <translation>第 %1 行: %2</translation>
    </message>
    <message>
        <source>TNCC Machine Certificates</source>
        <translation>TNCC 机器证书</translation>
    </message>
</context>
<context>
    <name>EditOpenVPN</name>
    <message>
        <source>&lt;html&gt;&lt;head/&gt;&lt;body&gt;&lt;p&gt;Default transport network. Leave empty to use the core default (udp).&lt;/p&gt;&lt;/body&gt;&lt;/html&gt;</source>
        <translation>&lt;html&gt;&lt;head/&gt;&lt;body&gt;&lt;p&gt;默认传输层网络(协议)。留空表示使用核心的默认设置 (udp)。&lt;/p&gt;&lt;/body&gt;&lt;/html&gt;</translation>
    </message>
    <message>
        <source>Network</source>
        <translation>网络协议</translation>
    </message>
    <message>
        <source>Username</source>
        <translation>用户名</translation>
    </message>
    <message>
        <source>Password</source>
        <translation>密码</translation>
    </message>
    <message>
        <source>&lt;html&gt;&lt;head/&gt;&lt;body&gt;&lt;p&gt;Static challenge text shown by the server when it asks for a second authentication response. When a challenge is set and an OTP profile is bound, Throne sends the answer itself and no interactive prompt appears.&lt;/p&gt;&lt;/body&gt;&lt;/html&gt;</source>
        <translation>&lt;html&gt;&lt;head/&gt;&lt;body&gt;&lt;p&gt;服务器在要求第二次身份验证响应时显示的静态质询文本。当设置了质询文本并绑定了 OTP 配置档时，Throne 将自行发送应答，而不出现交互式提示。&lt;/p&gt;&lt;/body&gt;&lt;/html&gt;</translation>
    </message>
    <message>
        <source>Static Challenge</source>
        <translation>静态质询</translation>
    </message>
    <message>
        <source>&lt;html&gt;&lt;head/&gt;&lt;body&gt;&lt;p&gt;Show the static challenge response as plain text.&lt;/p&gt;&lt;/body&gt;&lt;/html&gt;</source>
        <translation>&lt;html&gt;&lt;head/&gt;&lt;body&gt;&lt;p&gt;显示静态质询响应为纯文本。&lt;/p&gt;&lt;/body&gt;&lt;/html&gt;</translation>
    </message>
    <message>
        <source>Echo challenge response</source>
        <translation>回显质询响应</translation>
    </message>
    <message>
        <source>&lt;html&gt;&lt;head/&gt;&lt;body&gt;&lt;p&gt;Tunnel MTU. Empty uses 1500 until the server pushes a value.&lt;/p&gt;&lt;/body&gt;&lt;/html&gt;</source>
        <translation>&lt;html&gt;&lt;head/&gt;&lt;body&gt;&lt;p&gt;隧道 MTU。留空使用 1500，直至服务器推送一个值。&lt;/p&gt;&lt;/body&gt;&lt;/html&gt;</translation>
    </message>
    <message>
        <source>MTU</source>
        <translation>MTU</translation>
    </message>
    <message>
        <source>OTP</source>
        <translation>OTP</translation>
    </message>
    <message>
        <source>&lt;html&gt;&lt;head/&gt;&lt;body&gt;&lt;p&gt;Traffic that falls through to this profile reaches the tunnel only when the destination is inside a subnet the VPN server advertises; anything else is blocked. Routing rules that name this profile explicitly are not affected and always win.&lt;/p&gt;&lt;/body&gt;&lt;/html&gt;</source>
        <translation>&lt;html&gt;&lt;head/&gt;&lt;body&gt;&lt;p&gt;落入这个配置档的兜底(falls through)流量，仅当目标地址位于 VPN 服务器广告的子网内时才会进入该隧道；除此之外的任何流量将被拦截。而那些显式指定这个配置档的路由规则不受影响，且始终赢得优先级。&lt;/p&gt;&lt;/body&gt;&lt;/html&gt;</translation>
    </message>
    <message>
        <source>Only route advertised network</source>
        <translation>仅路由通告的网络</translation>
    </message>
    <message>
        <source>&lt;html&gt;&lt;head/&gt;&lt;body&gt;&lt;p&gt;How DNS uses the resolvers the VPN server pushes when this profile is started.&lt;/p&gt;&lt;p&gt;None: ignore them. Use this if the server claims every domain and breaks public DNS.&lt;br/&gt;Prefer: resolve the names the server claims - its split-DNS suffixes and search domains - through them, and everything else as usual.&lt;br/&gt;Strict: also send every remote DNS query to them instead of the remote DNS server, and fail the lookups they cannot answer. Direct lookups are unchanged.&lt;/p&gt;&lt;p&gt;As an endpoint in a routing profile, Prefer and Strict both resolve only the names the server claims.&lt;/p&gt;&lt;/body&gt;&lt;/html&gt;</source>
        <translation>&lt;html&gt;&lt;head/&gt;&lt;body&gt;&lt;p&gt;当启动这个配置档时，DNS 如何使用 VPN 服务器推送的解析器。&lt;/p&gt;&lt;p&gt;None：无，忽略它们。如果服务器声明了所有域名并导致公共 DNS 失效则使用这个选项。&lt;br/&gt;Prefer：优先，仅通过它们（解析器）解析服务器声明的名称（即其分流 DNS 后缀和搜索域），而其余的名称则照常解析。&lt;br/&gt;Strict：严格，也发送每个远程 DNS 查询到解析器（而不是远程 DNS 服务器），若它们无法应答则使解析失败。直接查询不受影响。&lt;/p&gt;&lt;p&gt;作为路由配置档中的端点的情况下，“优先”和“严格”均仅解析服务器声明的名称。&lt;/p&gt;&lt;/body&gt;&lt;/html&gt;</translation>
    </message>
    <message>
        <source>Tunnel DNS</source>
        <translation>隧道 DNS</translation>
    </message>
    <message>
        <source>Advanced</source>
        <translation>高级</translation>
    </message>
    <message>
        <source>TLS</source>
        <translation>TLS</translation>
    </message>
    <message>
        <source>&lt;html&gt;&lt;head/&gt;&lt;body&gt;&lt;p&gt;Trusted CA certificate content (PEM).&lt;/p&gt;&lt;/body&gt;&lt;/html&gt;</source>
        <translation>&lt;html&gt;&lt;head/&gt;&lt;body&gt;&lt;p&gt;受信任的 CA (颁发机构)证书内容 (PEM)。&lt;/p&gt;&lt;/body&gt;&lt;/html&gt;</translation>
    </message>
    <message>
        <source>CA Certificate</source>
        <translation>CA 证书</translation>
    </message>
    <message>
        <source>Not set</source>
        <translation>未设置</translation>
    </message>
    <message>
        <source>Client Certificate</source>
        <translation>客户端证书</translation>
    </message>
    <message>
        <source>Client Key</source>
        <translation>客户端钥匙</translation>
    </message>
    <message>
        <source>&lt;html&gt;&lt;head/&gt;&lt;body&gt;&lt;p&gt;OpenVPN tls-auth / tls-crypt / tls-crypt-v2 control channel wrapping. Disabled when the type is empty.&lt;/p&gt;&lt;/body&gt;&lt;/html&gt;</source>
        <translation>&lt;html&gt;&lt;head/&gt;&lt;body&gt;&lt;p&gt;OpenVPN tls-auth / tls-crypt / tls-crypt-v2 控制通道封装。类型为空时禁用。&lt;/p&gt;&lt;/body&gt;&lt;/html&gt;</translation>
    </message>
    <message>
        <source>Control Channel Wrap</source>
        <translation>控制通道封装</translation>
    </message>
    <message>
        <source>Type</source>
        <translation>类型</translation>
    </message>
    <message>
        <source>Key</source>
        <translation>钥匙</translation>
    </message>
    <message>
        <source>&lt;html&gt;&lt;head/&gt;&lt;body&gt;&lt;p&gt;tls-auth key direction. Only used when the type is tls_auth; the key is bidirectional when empty.&lt;/p&gt;&lt;/body&gt;&lt;/html&gt;</source>
        <translation>&lt;html&gt;&lt;head/&gt;&lt;body&gt;&lt;p&gt;tls-auth 钥匙方向。仅当类型为 tls_auth 时使用；留空时则钥匙双向使用。&lt;/p&gt;&lt;/body&gt;&lt;/html&gt;</translation>
    </message>
    <message>
        <source>Direction</source>
        <translation>方向</translation>
    </message>
    <message>
        <source>Prefer</source>
        <translation>优先</translation>
    </message>
    <message>
        <source>Strict</source>
        <translation>严格</translation>
    </message>
    <message>
        <source>&lt;html&gt;&lt;head/&gt;&lt;body&gt;&lt;p&gt;Binds an authenticator entry to this profile. %1 in the username or password is replaced with a generated code at connect time, and a static challenge is answered with the same code.&lt;/p&gt;&lt;/body&gt;&lt;/html&gt;</source>
        <translation>&lt;html&gt;&lt;head/&gt;&lt;body&gt;&lt;p&gt;绑定一个认证条目到这个配置档。在用户名或密码中的 %1 会在连接时替换为生成的代码，并用相同的代码应答静态质询。&lt;/p&gt;&lt;/body&gt;&lt;/html&gt;</translation>
    </message>
    <message>
        <source>%1 is replaced at connect time</source>
        <translation>%1 在连接时被替换</translation>
    </message>
    <message>
        <source>Control Channel Wrap Key</source>
        <translation>控制通道封装钥匙</translation>
    </message>
    <message>
        <source>Already set</source>
        <translation>已设置</translation>
    </message>
    <message>
        <source>Control Channel Wrap Type</source>
        <translation>控制通道封装类型</translation>
    </message>
    <message>
        <source>Control Channel Wrap Direction</source>
        <translation>控制通道封装方向</translation>
    </message>
    <message>
        <source>None</source>
        <translation>无</translation>
    </message>
</context>
<context>
    <name>EditOpenVPNAdvanced</name>
    <message>
        <source>OpenVPN Advanced</source>
        <translation>OpenVPN 高级</translation>
    </message>
    <message>
        <source>Session</source>
        <translation>会话</translation>
    </message>
    <message>
        <source>&lt;html&gt;&lt;head/&gt;&lt;body&gt;&lt;p&gt;OpenVPN session mode. tls is used by default; static_key is a deprecated mode without a TLS control channel.&lt;/p&gt;&lt;/body&gt;&lt;/html&gt;</source>
        <translation>&lt;html&gt;&lt;head/&gt;&lt;body&gt;&lt;p&gt;OpenVPN 会话模式，默认使用 TLS；static_key 是一种弃用的模式，没有 TLS 控制通道。&lt;/p&gt;&lt;/body&gt;&lt;/html&gt;</translation>
    </message>
    <message>
        <source>Mode</source>
        <translation>模式</translation>
    </message>
    <message>
        <source>&lt;html&gt;&lt;head/&gt;&lt;body&gt;&lt;p&gt;Tunnel topology. The topology pulled from the server is used when empty in TLS mode.&lt;/p&gt;&lt;/body&gt;&lt;/html&gt;</source>
        <translation>&lt;html&gt;&lt;head/&gt;&lt;body&gt;&lt;p&gt;隧道拓扑。TLS 模式下为空时，会使用从服务器获取的拓扑信息。&lt;/p&gt;&lt;/body&gt;&lt;/html&gt;</translation>
    </message>
    <message>
        <source>Topology</source>
        <translation>拓扑</translation>
    </message>
    <message>
        <source>&lt;html&gt;&lt;head/&gt;&lt;body&gt;&lt;p&gt;Behavior after username/password authentication fails. none is used by default and treats a permanent failure as terminal.&lt;/p&gt;&lt;/body&gt;&lt;/html&gt;</source>
        <translation>&lt;html&gt;&lt;head/&gt;&lt;body&gt;&lt;p&gt;用户名/密码认证失败后的处理行为。默认不采取任何操作；把永久性失败视为终止状态。&lt;/p&gt;&lt;/body&gt;&lt;/html&gt;</translation>
    </message>
    <message>
        <source>Auth Retry</source>
        <translation>认证重试</translation>
    </message>
    <message>
        <source>&lt;html&gt;&lt;head/&gt;&lt;body&gt;&lt;p&gt;Randomize the server list before connecting.&lt;/p&gt;&lt;/body&gt;&lt;/html&gt;</source>
        <translation>&lt;html&gt;&lt;head/&gt;&lt;body&gt;&lt;p&gt;连接前随机化服务器列表。&lt;/p&gt;&lt;/body&gt;&lt;/html&gt;</translation>
    </message>
    <message>
        <source>Randomize server order</source>
        <translation>随机化服务器顺序</translation>
    </message>
    <message>
        <source>&lt;html&gt;&lt;head/&gt;&lt;body&gt;&lt;p&gt;Comma separated local tunnel prefixes. At least one is required in static_key mode.&lt;/p&gt;&lt;/body&gt;&lt;/html&gt;</source>
        <translation>&lt;html&gt;&lt;head/&gt;&lt;body&gt;&lt;p&gt;逗号间隔的本地隧道前缀。在 static_key 模式下，至少需要填写一个。&lt;/p&gt;&lt;/body&gt;&lt;/html&gt;</translation>
    </message>
    <message>
        <source>Local Address</source>
        <translation>本地地址</translation>
    </message>
    <message>
        <source>10.8.0.2/24, fd00::2/64</source>
        <translation>10.8.0.2/24, fd00::2/64</translation>
    </message>
    <message>
        <source>&lt;html&gt;&lt;head/&gt;&lt;body&gt;&lt;p&gt;IPv4 tunnel peer address and VPN gateway. Required with an IPv4 local address in static_key mode.&lt;/p&gt;&lt;/body&gt;&lt;/html&gt;</source>
        <translation>&lt;html&gt;&lt;head/&gt;&lt;body&gt;&lt;p&gt;IPv4 隧道对端(peer)地址和 VPN 网关。在 static_key 模式下需要与 IPv4 本地地址一起使用。&lt;/p&gt;&lt;/body&gt;&lt;/html&gt;</translation>
    </message>
    <message>
        <source>Peer Address</source>
        <translation>对端地址</translation>
    </message>
    <message>
        <source>&lt;html&gt;&lt;head/&gt;&lt;body&gt;&lt;p&gt;IPv6 tunnel peer address and VPN gateway. Required with an IPv6 local address in static_key mode.&lt;/p&gt;&lt;/body&gt;&lt;/html&gt;</source>
        <translation>&lt;html&gt;&lt;head/&gt;&lt;body&gt;&lt;p&gt;IPv6 隧道对端(peer)地址和 VPN 网关。在 static_key 模式下需要与 IPv6 本地地址一起使用。&lt;/p&gt;&lt;/body&gt;&lt;/html&gt;</translation>
    </message>
    <message>
        <source>Peer Address (IPv6)</source>
        <translation>对端地址 (IPv6)</translation>
    </message>
    <message>
        <source>Servers</source>
        <translation>服务器</translation>
    </message>
    <message>
        <source>&lt;html&gt;&lt;head/&gt;&lt;body&gt;&lt;p&gt;Servers are tried in order and the client moves to the next one when a connection fails. Leave empty to use the address and port from the profile.&lt;/p&gt;&lt;/body&gt;&lt;/html&gt;</source>
        <translation>&lt;html&gt;&lt;head/&gt;&lt;body&gt;&lt;p&gt;服务器会按序尝试，客户端会在失败时换到下一个。留空表示使用来自配置档的地址和端口。&lt;/p&gt;&lt;/body&gt;&lt;/html&gt;</translation>
    </message>
    <message>
        <source>Server</source>
        <translation>服务器</translation>
    </message>
    <message>
        <source>Port</source>
        <translation>端口</translation>
    </message>
    <message>
        <source>Network</source>
        <translation>传输(网络)</translation>
    </message>
    <message>
        <source>Add</source>
        <translation>添加</translation>
    </message>
    <message>
        <source>Remove</source>
        <translation>删除</translation>
    </message>
    <message>
        <source>Static Key Mode</source>
        <translation>静态钥匙模式</translation>
    </message>
    <message>
        <source>&lt;html&gt;&lt;head/&gt;&lt;body&gt;&lt;p&gt;Static key content. Required in static_key mode. Conflicts with the static key path.&lt;/p&gt;&lt;/body&gt;&lt;/html&gt;</source>
        <translation>&lt;html&gt;&lt;head/&gt;&lt;body&gt;&lt;p&gt;静态钥匙(Static key)内容。在 static_key 模式下必需。与静态钥匙路径不能同时使用。&lt;/p&gt;&lt;/body&gt;&lt;/html&gt;</translation>
    </message>
    <message>
        <source>Static Key</source>
        <translation>静态钥匙</translation>
    </message>
    <message>
        <source>Not set</source>
        <translation>未设置</translation>
    </message>
    <message>
        <source>&lt;html&gt;&lt;head/&gt;&lt;body&gt;&lt;p&gt;Conflicts with the inline static key.&lt;/p&gt;&lt;/body&gt;&lt;/html&gt;</source>
        <translation>&lt;html&gt;&lt;head/&gt;&lt;body&gt;&lt;p&gt;与内联静态钥匙不能同时使用。&lt;/p&gt;&lt;/body&gt;&lt;/html&gt;</translation>
    </message>
    <message>
        <source>Static Key Path</source>
        <translation>静态钥匙路径</translation>
    </message>
    <message>
        <source>&lt;html&gt;&lt;head/&gt;&lt;body&gt;&lt;p&gt;Static key direction. The key is used bidirectionally when empty. Only available in static_key mode.&lt;/p&gt;&lt;/body&gt;&lt;/html&gt;</source>
        <translation>&lt;html&gt;&lt;head/&gt;&lt;body&gt;&lt;p&gt;静态钥匙方向。留空时则双向使用该钥匙。仅在 static_key 模式下可用。&lt;/p&gt;&lt;/body&gt;&lt;/html&gt;</translation>
    </message>
    <message>
        <source>Key Direction</source>
        <translation>钥匙方向</translation>
    </message>
    <message>
        <source>&lt;html&gt;&lt;head/&gt;&lt;body&gt;&lt;p&gt;Data channel cipher used in static_key mode. The upstream default BF-CBC is used when empty.&lt;/p&gt;&lt;/body&gt;&lt;/html&gt;</source>
        <translation>&lt;html&gt;&lt;head/&gt;&lt;body&gt;&lt;p&gt;在 static_key 模式下使用的数据通道密码算法。留空时使用上游默认的 BF-CBC。&lt;/p&gt;&lt;/body&gt;&lt;/html&gt;</translation>
    </message>
    <message>
        <source>Data Cipher</source>
        <translation>数据密码算法</translation>
    </message>
    <message>
        <source>TLS</source>
        <translation>TLS</translation>
    </message>
    <message>
        <source>&lt;html&gt;&lt;head/&gt;&lt;body&gt;&lt;p&gt;Expected server certificate name. Name verification is disabled when empty.&lt;/p&gt;&lt;/body&gt;&lt;/html&gt;</source>
        <translation>&lt;html&gt;&lt;head/&gt;&lt;body&gt;&lt;p&gt;预期的服务器证书名称。留空时禁用名称验证。&lt;/p&gt;&lt;/body&gt;&lt;/html&gt;</translation>
    </message>
    <message>
        <source>Server Name</source>
        <translation>服务器名称</translation>
    </message>
    <message>
        <source>&lt;html&gt;&lt;head/&gt;&lt;body&gt;&lt;p&gt;Certificate field matched by the server name. name is used by default and matches the common name exactly.&lt;/p&gt;&lt;/body&gt;&lt;/html&gt;</source>
        <translation>&lt;html&gt;&lt;head/&gt;&lt;body&gt;&lt;p&gt;被服务器名称匹配的证书字段。默认使用“name”（名称）并精确匹配“common name”（通用名称）。&lt;/p&gt;&lt;/body&gt;&lt;/html&gt;</translation>
    </message>
    <message>
        <source>Server Name Type</source>
        <translation>服务器名称类型</translation>
    </message>
    <message>
        <source>&lt;html&gt;&lt;head/&gt;&lt;body&gt;&lt;p&gt;Conflicts with the inline CA certificate on the main editor.&lt;/p&gt;&lt;/body&gt;&lt;/html&gt;</source>
        <translation>&lt;html&gt;&lt;head/&gt;&lt;body&gt;&lt;p&gt;与主编辑器上的内联 CA 证书冲突（不能同时使用）。&lt;/p&gt;&lt;/body&gt;&lt;/html&gt;</translation>
    </message>
    <message>
        <source>CA Certificate Path</source>
        <translation>CA 证书路径</translation>
    </message>
    <message>
        <source>&lt;html&gt;&lt;head/&gt;&lt;body&gt;&lt;p&gt;Conflicts with the inline client certificate on the main editor.&lt;/p&gt;&lt;/body&gt;&lt;/html&gt;</source>
        <translation>&lt;html&gt;&lt;head/&gt;&lt;body&gt;&lt;p&gt;与主编辑器上的内联客户端证书冲突（不能同时使用）。&lt;/p&gt;&lt;/body&gt;&lt;/html&gt;</translation>
    </message>
    <message>
        <source>Client Certificate Path</source>
        <translation>客户端证书路径</translation>
    </message>
    <message>
        <source>&lt;html&gt;&lt;head/&gt;&lt;body&gt;&lt;p&gt;Conflicts with the inline client key on the main editor.&lt;/p&gt;&lt;/body&gt;&lt;/html&gt;</source>
        <translation>&lt;html&gt;&lt;head/&gt;&lt;body&gt;&lt;p&gt;与主编辑器上的内联客户端密钥冲突（不能同时使用）。&lt;/p&gt;&lt;/body&gt;&lt;/html&gt;</translation>
    </message>
    <message>
        <source>Client Key Path</source>
        <translation>客户端钥匙路径</translation>
    </message>
    <message>
        <source>&lt;html&gt;&lt;head/&gt;&lt;body&gt;&lt;p&gt;Conflicts with the inline control channel wrap key on the main editor.&lt;/p&gt;&lt;/body&gt;&lt;/html&gt;</source>
        <translation>&lt;html&gt;&lt;head/&gt;&lt;body&gt;&lt;p&gt;与主要编辑器上控制通道 wrap 钥匙不能同时使用。&lt;/p&gt;&lt;/body&gt;&lt;/html&gt;</translation>
    </message>
    <message>
        <source>Control Wrap Key Path</source>
        <translation>控制 Wrap 钥匙路径</translation>
    </message>
    <message>
        <source>&lt;html&gt;&lt;head/&gt;&lt;body&gt;&lt;p&gt;Allowed SHA-256 fingerprints of the server leaf certificate, one per line, 64 lowercase hexadecimal characters without separators.&lt;/p&gt;&lt;/body&gt;&lt;/html&gt;</source>
        <translation>&lt;html&gt;&lt;head/&gt;&lt;body&gt;&lt;p&gt;允许的服务器叶证书的 SHA-256 指纹，每行一个，必须是 64 个小写十六进制字符，且不含分隔符。&lt;/p&gt;&lt;/body&gt;&lt;/html&gt;</translation>
    </message>
    <message>
        <source>Peer Fingerprint</source>
        <translation>对端指纹</translation>
    </message>
    <message>
        <source>&lt;html&gt;&lt;head/&gt;&lt;body&gt;&lt;p&gt;PEM or DER certificate revocation list used to reject revoked server certificates.&lt;/p&gt;&lt;/body&gt;&lt;/html&gt;</source>
        <translation>&lt;html&gt;&lt;head/&gt;&lt;body&gt;&lt;p&gt;用于拒绝已撤销服务器证书的 PEM 或 DER 格式的 CRL (Certificate Revocation List 证书吊销列表)。&lt;/p&gt;&lt;/body&gt;&lt;/html&gt;</translation>
    </message>
    <message>
        <source>CRL Path</source>
        <translation>CRL 路径</translation>
    </message>
    <message>
        <source>&lt;html&gt;&lt;head/&gt;&lt;body&gt;&lt;p&gt;Comma separated hexadecimal key usage masks in OpenVPN remote-cert-ku format.&lt;/p&gt;&lt;/body&gt;&lt;/html&gt;</source>
        <translation>&lt;html&gt;&lt;head/&gt;&lt;body&gt;&lt;p&gt;逗号间隔的十六进制钥匙用法掩码，格式为 OpenVPN `remote-cert-ku` 格式。&lt;/p&gt;&lt;/body&gt;&lt;/html&gt;</translation>
    </message>
    <message>
        <source>Remote Certificate KU</source>
        <translation>远程证书钥匙用法</translation>
    </message>
    <message>
        <source>&lt;html&gt;&lt;head/&gt;&lt;body&gt;&lt;p&gt;Required extended key usage. Conflicts with an explicit remote certificate TLS check.&lt;/p&gt;&lt;/body&gt;&lt;/html&gt;</source>
        <translation>&lt;html&gt;&lt;head/&gt;&lt;body&gt;&lt;p&gt;需要 EKU (Extended Key Usage 扩展钥匙用法)。与显式远程证书 TLS 检查冲突（不能同时使用）。&lt;/p&gt;&lt;/body&gt;&lt;/html&gt;</translation>
    </message>
    <message>
        <source>Remote Certificate EKU</source>
        <translation>远程证书扩展钥匙用法</translation>
    </message>
    <message>
        <source>&lt;html&gt;&lt;head/&gt;&lt;body&gt;&lt;p&gt;Peer certificate purpose check. server is used by default; none disables the check.&lt;/p&gt;&lt;/body&gt;&lt;/html&gt;</source>
        <translation>&lt;html&gt;&lt;head/&gt;&lt;body&gt;&lt;p&gt;对端证书用途检查。默认使用 server (服务器)；选择 none (无)将禁用该检查。&lt;/p&gt;&lt;/body&gt;&lt;/html&gt;</translation>
    </message>
    <message>
        <source>Remote Certificate TLS</source>
        <translation>远程证书 TLS</translation>
    </message>
    <message>
        <source>&lt;html&gt;&lt;head/&gt;&lt;body&gt;&lt;p&gt;legacy is used by default. insecure accepts MD5 and SHA-1 signed chains; preferred requires stronger signatures and keys.&lt;/p&gt;&lt;/body&gt;&lt;/html&gt;</source>
        <translation>&lt;html&gt;&lt;head/&gt;&lt;body&gt;&lt;p&gt;默认使用 &quot;legacy&quot;。insecure 接受 MD5 和 SHA-1 签名的（证书）链；preferred 需要更强的签名和钥匙。&lt;/p&gt;&lt;/body&gt;&lt;/html&gt;</translation>
    </message>
    <message>
        <source>Certificate Profile</source>
        <translation>证书配置方案</translation>
    </message>
    <message>
        <source>&lt;html&gt;&lt;head/&gt;&lt;body&gt;&lt;p&gt;Deprecated Netscape certificate type check. Prefer Remote Certificate TLS.&lt;/p&gt;&lt;/body&gt;&lt;/html&gt;</source>
        <translation>&lt;html&gt;&lt;head/&gt;&lt;body&gt;&lt;p&gt;已弃用的 Netscape 证书类型检查。建议使用“远程证书 TLS”。&lt;/p&gt;&lt;/body&gt;&lt;/html&gt;</translation>
    </message>
    <message>
        <source>NS Certificate Type</source>
        <translation>NS 证书类型</translation>
    </message>
    <message>
        <source>&lt;html&gt;&lt;head/&gt;&lt;body&gt;&lt;p&gt;1.2 is used by default.&lt;/p&gt;&lt;/body&gt;&lt;/html&gt;</source>
        <translation>&lt;html&gt;&lt;head/&gt;&lt;body&gt;&lt;p&gt;默认使用 1.2。&lt;/p&gt;&lt;/body&gt;&lt;/html&gt;</translation>
    </message>
    <message>
        <source>Min TLS Version</source>
        <translation>最小 TLS 版本</translation>
    </message>
    <message>
        <source>&lt;html&gt;&lt;head/&gt;&lt;body&gt;&lt;p&gt;The maximum supported version is used by default. It cannot be lower than the minimum.&lt;/p&gt;&lt;/body&gt;&lt;/html&gt;</source>
        <translation>&lt;html&gt;&lt;head/&gt;&lt;body&gt;&lt;p&gt;默认使用支持的最高版本。它不能低于最低版本。&lt;/p&gt;&lt;/body&gt;&lt;/html&gt;</translation>
    </message>
    <message>
        <source>Max TLS Version</source>
        <translation>最大 TLS 版本</translation>
    </message>
    <message>
        <source>&lt;html&gt;&lt;head/&gt;&lt;body&gt;&lt;p&gt;Colon separated OpenSSL cipher suite names allowed for TLS 1.2 and earlier.&lt;/p&gt;&lt;/body&gt;&lt;/html&gt;</source>
        <translation>&lt;html&gt;&lt;head/&gt;&lt;body&gt;&lt;p&gt;允许用于 TLS 1.2 及更早版本的以冒号间隔的 OpenSSL 密码（算法）套件名称。&lt;/p&gt;&lt;/body&gt;&lt;/html&gt;</translation>
    </message>
    <message>
        <source>TLS Cipher</source>
        <translation>TLS 密码套件</translation>
    </message>
    <message>
        <source>&lt;html&gt;&lt;head/&gt;&lt;body&gt;&lt;p&gt;Colon separated key exchange groups in preference order: X25519, SECP256R1, SECP384R1, SECP521R1.&lt;/p&gt;&lt;/body&gt;&lt;/html&gt;</source>
        <translation>&lt;html&gt;&lt;head/&gt;&lt;body&gt;&lt;p&gt;按偏好顺序以冒号间隔的密钥交换组：X25519、SECP256R1、SECP384R1、SECP521R1。&lt;/p&gt;&lt;/body&gt;&lt;/html&gt;</translation>
    </message>
    <message>
        <source>TLS Groups</source>
        <translation>TLS 组</translation>
    </message>
    <message>
        <source>Data Channel</source>
        <translation>数据通道</translation>
    </message>
    <message>
        <source>&lt;html&gt;&lt;head/&gt;&lt;body&gt;&lt;p&gt;Comma separated allowed data channel ciphers. Only available in TLS mode.&lt;/p&gt;&lt;/body&gt;&lt;/html&gt;</source>
        <translation>&lt;html&gt;&lt;head/&gt;&lt;body&gt;&lt;p&gt;逗号间隔的允许的数据通道密码算法。仅在 TLS 模式下可用。&lt;/p&gt;&lt;/body&gt;&lt;/html&gt;</translation>
    </message>
    <message>
        <source>Data Ciphers</source>
        <translation>数据密码算法</translation>
    </message>
    <message>
        <source>AES-256-GCM,AES-128-GCM,CHACHA20-POLY1305</source>
        <translation>AES-256-GCM,AES-128-GCM,CHACHA20-POLY1305</translation>
    </message>
    <message>
        <source>&lt;html&gt;&lt;head/&gt;&lt;body&gt;&lt;p&gt;Data channel cipher for peers that do not support cipher negotiation.&lt;/p&gt;&lt;/body&gt;&lt;/html&gt;</source>
        <translation>&lt;html&gt;&lt;head/&gt;&lt;body&gt;&lt;p&gt;用于不支持密码算法协商的对端的数据通道密码算法。&lt;/p&gt;&lt;/body&gt;&lt;/html&gt;</translation>
    </message>
    <message>
        <source>Data Ciphers Fallback</source>
        <translation>数据密码算法回退</translation>
    </message>
    <message>
        <source>&lt;html&gt;&lt;head/&gt;&lt;body&gt;&lt;p&gt;Data channel authentication digest. SHA1 is used by default and only applies to non-AEAD ciphers and tls_auth.&lt;/p&gt;&lt;/body&gt;&lt;/html&gt;</source>
        <translation>&lt;html&gt;&lt;head/&gt;&lt;body&gt;&lt;p&gt;数据通道认证摘要算法(digest)。默认使用 SHA1，仅适用于非 AEAD 密码算法和 tls_auth。&lt;/p&gt;&lt;/body&gt;&lt;/html&gt;</translation>
    </message>
    <message>
        <source>Auth Digest</source>
        <translation>认证摘要算法</translation>
    </message>
    <message>
        <source>&lt;html&gt;&lt;head/&gt;&lt;body&gt;&lt;p&gt;Maximum UDP packet size used to clamp the MSS of TCP connections sent through the tunnel.&lt;/p&gt;&lt;/body&gt;&lt;/html&gt;</source>
        <translation>&lt;html&gt;&lt;head/&gt;&lt;body&gt;&lt;p&gt;用于限制通过隧道发送的 TCP 连接的 MSS 的最大 UDP 数据包大小。&lt;/p&gt;&lt;/body&gt;&lt;/html&gt;</translation>
    </message>
    <message>
        <source>MSS Fix</source>
        <translation>MSS 修正</translation>
    </message>
    <message>
        <source>&lt;html&gt;&lt;head/&gt;&lt;body&gt;&lt;p&gt;Disables MSS clamping including the default clamp. Conflicts with MSS Fix and MSS Fix Mode.&lt;/p&gt;&lt;/body&gt;&lt;/html&gt;</source>
        <translation>&lt;html&gt;&lt;head/&gt;&lt;body&gt;&lt;p&gt;禁用 MSS 限制，包括默认限制。与 MSS Fix (修正)和 MSS Fix Mode (修正模式)冲突（不能同时使用）。&lt;/p&gt;&lt;/body&gt;&lt;/html&gt;</translation>
    </message>
    <message>
        <source>Disable MSS clamping</source>
        <translation>禁用 MSS 限制</translation>
    </message>
    <message>
        <source>&lt;html&gt;&lt;head/&gt;&lt;body&gt;&lt;p&gt;mtu also accounts for the outer IP and transport headers; fixed treats MSS Fix as an inner IPv4 packet size. Requires MSS Fix.&lt;/p&gt;&lt;/body&gt;&lt;/html&gt;</source>
        <translation>&lt;html&gt;&lt;head/&gt;&lt;body&gt;&lt;p&gt;MTU 同样计入外部 IP 和传输层标头；`fixed` 将 MSS 修正视为内层 IPv4 数据包大小。需要（启用） MSS 修正。&lt;/p&gt;&lt;/body&gt;&lt;/html&gt;</translation>
    </message>
    <message>
        <source>MSS Fix Mode</source>
        <translation>MSS 修正模式</translation>
    </message>
    <message>
        <source>&lt;html&gt;&lt;head/&gt;&lt;body&gt;&lt;p&gt;Maximum UDP packet size used for data channel fragmentation. A non-zero value must be at least 68. Conflicts with TCP transport.&lt;/p&gt;&lt;/body&gt;&lt;/html&gt;</source>
        <translation>&lt;html&gt;&lt;head/&gt;&lt;body&gt;&lt;p&gt;用于数据通道分片的最大 UDP 数据包大小。非零值必须至少为 68。与 TCP 传输冲突（不能同时使用）。&lt;/p&gt;&lt;/body&gt;&lt;/html&gt;</translation>
    </message>
    <message>
        <source>Fragment</source>
        <translation>分片</translation>
    </message>
    <message>
        <source>&lt;html&gt;&lt;head/&gt;&lt;body&gt;&lt;p&gt;UDP replay window size. 64 is used by default and the maximum is 65536.&lt;/p&gt;&lt;/body&gt;&lt;/html&gt;</source>
        <translation>&lt;html&gt;&lt;head/&gt;&lt;body&gt;&lt;p&gt;UDP 重放窗口大小。默认使用 64，最大值为 65536。&lt;/p&gt;&lt;/body&gt;&lt;/html&gt;</translation>
    </message>
    <message>
        <source>Replay Window</source>
        <translation>重放窗口</translation>
    </message>
    <message>
        <source>&lt;html&gt;&lt;head/&gt;&lt;body&gt;&lt;p&gt;UDP replay window duration in whole seconds. The maximum is 10m.&lt;/p&gt;&lt;/body&gt;&lt;/html&gt;</source>
        <translation>&lt;html&gt;&lt;head/&gt;&lt;body&gt;&lt;p&gt;UDP 重放窗口持续时间(以整秒为单位)。最大值为 10 分钟。&lt;/p&gt;&lt;/body&gt;&lt;/html&gt;</translation>
    </message>
    <message>
        <source>Replay Window Time</source>
        <translation>重放窗口时间</translation>
    </message>
    <message>
        <source>15s (default)</source>
        <translation>15s (15秒，默认)</translation>
    </message>
    <message>
        <source>Compression</source>
        <translation>压缩</translation>
    </message>
    <message>
        <source>&lt;html&gt;&lt;head/&gt;&lt;body&gt;&lt;p&gt;OpenVPN compress framing mode. Disabled by default. Compression can weaken traffic confidentiality.&lt;/p&gt;&lt;/body&gt;&lt;/html&gt;</source>
        <translation>&lt;html&gt;&lt;head/&gt;&lt;body&gt;&lt;p&gt;OpenVPN 压缩帧模式。默认禁用。压缩可能削弱流量的机密性。&lt;/p&gt;&lt;/body&gt;&lt;/html&gt;</translation>
    </message>
    <message>
        <source>&lt;html&gt;&lt;head/&gt;&lt;body&gt;&lt;p&gt;OpenVPN comp-lzo mode. Disabled by default. Enable it only when required by the server.&lt;/p&gt;&lt;/body&gt;&lt;/html&gt;</source>
        <translation>&lt;html&gt;&lt;head/&gt;&lt;body&gt;&lt;p&gt;OpenVPN comp-lzo 模式。默认禁用。仅在服务器要求时启用。&lt;/p&gt;&lt;/body&gt;&lt;/html&gt;</translation>
    </message>
    <message>
        <source>Compression LZO</source>
        <translation>压缩 LZO</translation>
    </message>
    <message>
        <source>&lt;html&gt;&lt;head/&gt;&lt;body&gt;&lt;p&gt;Policy for compression pushed by the server. no is used by default and permits only stub framing.&lt;/p&gt;&lt;/body&gt;&lt;/html&gt;</source>
        <translation>&lt;html&gt;&lt;head/&gt;&lt;body&gt;&lt;p&gt;服务器推送的压缩处理策略。默认使用 no，仅允许 stub (存根)帧格式。&lt;/p&gt;&lt;/body&gt;&lt;/html&gt;</translation>
    </message>
    <message>
        <source>Allow Compression</source>
        <translation>允许压缩</translation>
    </message>
    <message>
        <source>Routing</source>
        <translation>路由</translation>
    </message>
    <message>
        <source>&lt;html&gt;&lt;head/&gt;&lt;body&gt;&lt;p&gt;Ignores routes, DNS, route metrics, redirect-gateway, redirect-private and block-ipv6 pushed by the server.&lt;/p&gt;&lt;/body&gt;&lt;/html&gt;</source>
        <translation>&lt;html&gt;&lt;head/&gt;&lt;body&gt;&lt;p&gt;忽略服务器推送的路由、DNS、路由度量、redirect-gateway (重定向网关)、redirect-private (重定向私有)以及 block-ipv6 (阻止 IPv6) 设置。&lt;/p&gt;&lt;/body&gt;&lt;/html&gt;</translation>
    </message>
    <message>
        <source>Ignore pushed routes, DNS and gateway options</source>
        <translation>忽略推送的路由、DNS 和网关选项</translation>
    </message>
    <message>
        <source>&lt;html&gt;&lt;head/&gt;&lt;body&gt;&lt;p&gt;Comma separated prefixes preferred for this endpoint, in addition to routes accepted from the server.&lt;/p&gt;&lt;/body&gt;&lt;/html&gt;</source>
        <translation>&lt;html&gt;&lt;head/&gt;&lt;body&gt;&lt;p&gt;这个端点首选的前缀（逗号间隔），此外还使用接收自服务器的路由。&lt;/p&gt;&lt;/body&gt;&lt;/html&gt;</translation>
    </message>
    <message>
        <source>Routes</source>
        <translation>路由</translation>
    </message>
    <message>
        <source>&lt;html&gt;&lt;head/&gt;&lt;body&gt;&lt;p&gt;IPv4 gateway for routes through the endpoint. The gateway received from the server is used when empty.&lt;/p&gt;&lt;/body&gt;&lt;/html&gt;</source>
        <translation>&lt;html&gt;&lt;head/&gt;&lt;body&gt;&lt;p&gt;通过该端点的路由的 IPv4 网关。为空时使用从服务器接收的网关。&lt;/p&gt;&lt;/body&gt;&lt;/html&gt;</translation>
    </message>
    <message>
        <source>Route Gateway</source>
        <translation>路由网关</translation>
    </message>
    <message>
        <source>&lt;html&gt;&lt;head/&gt;&lt;body&gt;&lt;p&gt;Default metric for routes through the endpoint. The platform default is used when empty.&lt;/p&gt;&lt;/body&gt;&lt;/html&gt;</source>
        <translation>&lt;html&gt;&lt;head/&gt;&lt;body&gt;&lt;p&gt;通过该端点的路由的默认度量值。为空时使用平台默认值。&lt;/p&gt;&lt;/body&gt;&lt;/html&gt;</translation>
    </message>
    <message>
        <source>Route Metric</source>
        <translation>路由度量</translation>
    </message>
    <message>
        <source>&lt;html&gt;&lt;head/&gt;&lt;body&gt;&lt;p&gt;Does not install an operating system default route.&lt;/p&gt;&lt;/body&gt;&lt;/html&gt;</source>
        <translation>&lt;html&gt;&lt;head/&gt;&lt;body&gt;&lt;p&gt;不安装操作系统默认路由。&lt;/p&gt;&lt;/body&gt;&lt;/html&gt;</translation>
    </message>
    <message>
        <source>Prefer this endpoint for all IPv4 destinations</source>
        <translation>所有 IPv4 目标首选这个端点</translation>
    </message>
    <message>
        <source>&lt;html&gt;&lt;head/&gt;&lt;body&gt;&lt;p&gt;Comma separated OpenVPN redirect-gateway flags, for example !ipv4, def1 or ipv6.&lt;/p&gt;&lt;/body&gt;&lt;/html&gt;</source>
        <translation>&lt;html&gt;&lt;head/&gt;&lt;body&gt;&lt;p&gt;逗号间隔的 OpenVPN redirect-gateway (重定向网关)标志，例如 !ipv4、def1 或 ipv6。&lt;/p&gt;&lt;/body&gt;&lt;/html&gt;</translation>
    </message>
    <message>
        <source>Redirect Gateway Flags</source>
        <translation>“重定向网关”标志</translation>
    </message>
    <message>
        <source>Accept redirect gateway flags without a default route preference</source>
        <translation>接受 redirect gateway (重定向网关)标志而不使用默认路由偏好</translation>
    </message>
    <message>
        <source>Reject IPv6 traffic locally instead of sending it through the VPN</source>
        <translation>本地拒绝 IPv6 流量，而不是通过 VPN 发送它</translation>
    </message>
    <message>
        <source>&lt;html&gt;&lt;head/&gt;&lt;body&gt;&lt;p&gt;Ordered filters for options pushed by the server. The first filter whose text is a case-sensitive prefix of the complete pushed option is applied.&lt;/p&gt;&lt;/body&gt;&lt;/html&gt;</source>
        <translation>&lt;html&gt;&lt;head/&gt;&lt;body&gt;&lt;p&gt;用于服务器推送选项的有序过滤器。（过滤器）文本是完整推送选项的区分大小写的前缀的第一个过滤器将被应用。&lt;/p&gt;&lt;/body&gt;&lt;/html&gt;</translation>
    </message>
    <message>
        <source>Action</source>
        <translation>动作</translation>
    </message>
    <message>
        <source>Text</source>
        <translation>文本</translation>
    </message>
    <message>
        <source>Timers</source>
        <translation>计时器</translation>
    </message>
    <message>
        <source>&lt;html&gt;&lt;head/&gt;&lt;body&gt;&lt;p&gt;Whole seconds of silence before a data channel ping is sent. A server-pushed value overrides it.&lt;/p&gt;&lt;/body&gt;&lt;/html&gt;</source>
        <translation>&lt;html&gt;&lt;head/&gt;&lt;body&gt;&lt;p&gt;在发送数据通道 ping 前的静默整秒数。服务器推送值优先于它。&lt;/p&gt;&lt;/body&gt;&lt;/html&gt;</translation>
    </message>
    <message>
        <source>Ping Interval</source>
        <translation>Ping 间隔</translation>
    </message>
    <message>
        <source>&lt;html&gt;&lt;head/&gt;&lt;body&gt;&lt;p&gt;Whole seconds without a received packet before reconnecting. 120s is used for UDP with pull enabled.&lt;/p&gt;&lt;/body&gt;&lt;/html&gt;</source>
        <translation>&lt;html&gt;&lt;head/&gt;&lt;body&gt;&lt;p&gt;重新连接前允许没有收到数据包的整秒数。启用 pull (拉取)的 UDP 使用 120 秒。&lt;/p&gt;&lt;/body&gt;&lt;/html&gt;</translation>
    </message>
    <message>
        <source>Ping Restart</source>
        <translation>Ping 重启</translation>
    </message>
    <message>
        <source>&lt;html&gt;&lt;head/&gt;&lt;body&gt;&lt;p&gt;Disables the initial 120s UDP pull timeout and any configured ping restart. Conflicts with Ping Restart.&lt;/p&gt;&lt;/body&gt;&lt;/html&gt;</source>
        <translation>&lt;html&gt;&lt;head/&gt;&lt;body&gt;&lt;p&gt;禁用初始 120 秒 UDP pull (拉取)超时以及任何已配置的 ping 重启。与 Ping 重启冲突（不能同时使用）。&lt;/p&gt;&lt;/body&gt;&lt;/html&gt;</translation>
    </message>
    <message>
        <source>Disable ping restart</source>
        <translation>禁用 ping 重启</translation>
    </message>
    <message>
        <source>&lt;html&gt;&lt;head/&gt;&lt;body&gt;&lt;p&gt;TLS renegotiation interval.&lt;/p&gt;&lt;/body&gt;&lt;/html&gt;</source>
        <translation>&lt;html&gt;&lt;head/&gt;&lt;body&gt;&lt;p&gt;TLS 重新协商间隔。&lt;/p&gt;&lt;/body&gt;&lt;/html&gt;</translation>
    </message>
    <message>
        <source>Renegotiate Interval</source>
        <translation>重新协商间隔</translation>
    </message>
    <message>
        <source>1h (default)</source>
        <translation>1h (1小时，默认)</translation>
    </message>
    <message>
        <source>&lt;html&gt;&lt;head/&gt;&lt;body&gt;&lt;p&gt;Conflicts with Renegotiate Interval.&lt;/p&gt;&lt;/body&gt;&lt;/html&gt;</source>
        <translation>&lt;html&gt;&lt;head/&gt;&lt;body&gt;&lt;p&gt;与“重新协商间隔”冲突（不能同时使用）。&lt;/p&gt;&lt;/body&gt;&lt;/html&gt;</translation>
    </message>
    <message>
        <source>Disable time based renegotiation</source>
        <translation>禁用基于时间的重新协商</translation>
    </message>
    <message>
        <source>&lt;html&gt;&lt;head/&gt;&lt;body&gt;&lt;p&gt;Renegotiate data channel keys after this many bytes. Empty uses the cipher-dependent default.&lt;/p&gt;&lt;/body&gt;&lt;/html&gt;</source>
        <translation>&lt;html&gt;&lt;head/&gt;&lt;body&gt;&lt;p&gt;在传输指定字节数后重新协商数据通道密钥。为空时使用取决于密码算法的默认值。&lt;/p&gt;&lt;/body&gt;&lt;/html&gt;</translation>
    </message>
    <message>
        <source>Renegotiate Bytes</source>
        <translation>重新协商字节数</translation>
    </message>
    <message>
        <source>&lt;html&gt;&lt;head/&gt;&lt;body&gt;&lt;p&gt;Renegotiate data channel keys after this many packets. Empty uses the cipher-dependent default.&lt;/p&gt;&lt;/body&gt;&lt;/html&gt;</source>
        <translation>&lt;html&gt;&lt;head/&gt;&lt;body&gt;&lt;p&gt;在传输指定数量数据包后重新协商数据通道密钥。为空则使用取决于密码算法的默认值。&lt;/p&gt;&lt;/body&gt;&lt;/html&gt;</translation>
    </message>
    <message>
        <source>Renegotiate Packets</source>
        <translation>重新协商数据包数</translation>
    </message>
    <message>
        <source>&lt;html&gt;&lt;head/&gt;&lt;body&gt;&lt;p&gt;Initial retransmission timeout for TLS control packets.&lt;/p&gt;&lt;/body&gt;&lt;/html&gt;</source>
        <translation>&lt;html&gt;&lt;head/&gt;&lt;body&gt;&lt;p&gt;TLS 控制数据包的初始重传超时。&lt;/p&gt;&lt;/body&gt;&lt;/html&gt;</translation>
    </message>
    <message>
        <source>TLS Timeout</source>
        <translation>TLS 超时</translation>
    </message>
    <message>
        <source>2s (default)</source>
        <translation>2s (2秒，默认)</translation>
    </message>
    <message>
        <source>&lt;html&gt;&lt;head/&gt;&lt;body&gt;&lt;p&gt;Maximum time allowed for the initial handshake and each renegotiation.&lt;/p&gt;&lt;/body&gt;&lt;/html&gt;</source>
        <translation>&lt;html&gt;&lt;head/&gt;&lt;body&gt;&lt;p&gt;初始握手和每次重新协商允许的最长时间。&lt;/p&gt;&lt;/body&gt;&lt;/html&gt;</translation>
    </message>
    <message>
        <source>Handshake Window</source>
        <translation>握手窗口</translation>
    </message>
    <message>
        <source>1m (default)</source>
        <translation>1m (1分钟，默认)</translation>
    </message>
    <message>
        <source>&lt;html&gt;&lt;head/&gt;&lt;body&gt;&lt;p&gt;Number of exit notifications sent one second apart when closing a UDP connection.&lt;/p&gt;&lt;/body&gt;&lt;/html&gt;</source>
        <translation>&lt;html&gt;&lt;head/&gt;&lt;body&gt;&lt;p&gt;关闭 UDP 连接时发送的退出通知数量(每条通知间隔一秒)。&lt;/p&gt;&lt;/body&gt;&lt;/html&gt;</translation>
    </message>
    <message>
        <source>Explicit Exit Notify</source>
        <translation>显式退出通知</translation>
    </message>
    <message>
        <source>Already set</source>
        <translation>已设置</translation>
    </message>
    <message>
        <source>Row %1, host: %2</source>
        <translation>第 %1 行，主机: %2</translation>
    </message>
    <message>
        <source>Row %1, port: %2</source>
        <translation>第 %1 行, 端口: %2</translation>
    </message>
    <message>
        <source>Row %1 has no host, so it is not saved.</source>
        <translation>第 %1 行没有主机，所以没有被保存。</translation>
    </message>
    <message>
        <source>Row %1 has no text, so it is not saved.</source>
        <translation>第 %1 行没有文本，所以没有被保存。</translation>
    </message>
    <message>
        <source>Pull Filters</source>
        <translation>拉取过滤</translation>
    </message>
    <message>
        <source>Static key mode does not use the credentials, certificates or control channel wrap on the profile page; clear them there.</source>
        <translation>静态钥匙模式不使用配置档页面上的凭证、证书或控制通道封装；在那里清空它们。</translation>
    </message>
</context>
<context>
    <name>EditSSH</name>
    <message>
        <source>User</source>
        <translation>用户</translation>
    </message>
    <message>
        <source>Password</source>
        <translation>密码</translation>
    </message>
    <message>
        <source>Private Key</source>
        <translation>私钥</translation>
    </message>
    <message>
        <source>Private Key Password</source>
        <translation>私钥密码</translation>
    </message>
    <message>
        <source>Host Key</source>
        <translation>主机钥匙</translation>
    </message>
    <message>
        <source>Host Key Algorithms</source>
        <translation>主机钥匙算法</translation>
    </message>
    <message>
        <source>Client Version</source>
        <translation>客户端版本</translation>
    </message>
    <message>
        <source>Private Key Path</source>
        <translation>私钥路径</translation>
    </message>
    <message>
        <source>Choose File</source>
        <translation>选择文件</translation>
    </message>
</context>
<context>
    <name>EditShadowSocks</name>
    <message>
        <source>Plugin</source>
        <translation>插件</translation>
    </message>
    <message>
        <source>Encryption</source>
        <translation>加密</translation>
    </message>
    <message>
        <source>Plugin Args</source>
        <translation>插件参数</translation>
    </message>
    <message>
        <source>Password</source>
        <translation>密码</translation>
    </message>
    <message>
        <source>UDP over TCP</source>
        <translation>基于 TCP 的 UDP</translation>
    </message>
</context>
<context>
    <name>EditShadowTLS</name>
    <message>
        <source>Password</source>
        <translation>密码</translation>
    </message>
    <message>
        <source>Version</source>
        <translation>版本</translation>
    </message>
</context>
<context>
    <name>EditSnell</name>
    <message>
        <source>Version</source>
        <translation>版本</translation>
    </message>
    <message>
        <source>PSK</source>
        <translation>PSK</translation>
    </message>
    <message>
        <source>&lt;html&gt;&lt;head/&gt;&lt;body&gt;&lt;p&gt;Per-user key on a multi-user server. Leave empty when the server only has a PSK.&lt;/p&gt;&lt;/body&gt;&lt;/html&gt;</source>
        <translation>&lt;html&gt;&lt;head/&gt;&lt;body&gt;&lt;p&gt;多用户服务器的每用户专属钥匙。当服务器仅有 PSK (Pre-Shared Key 预共享密钥)时留空。&lt;/p&gt;&lt;/body&gt;&lt;/html&gt;</translation>
    </message>
    <message>
        <source>User Key</source>
        <translation>用户钥匙</translation>
    </message>
    <message>
        <source>&lt;html&gt;&lt;head/&gt;&lt;body&gt;&lt;p&gt;Restrict this outbound to one network. Leave empty to carry both TCP and UDP.&lt;/p&gt;&lt;/body&gt;&lt;/html&gt;</source>
        <translation>&lt;html&gt;&lt;head/&gt;&lt;body&gt;&lt;p&gt;限制这个出站到某个网络（协议）。留空表示同时承载 TCP 和 UDP。&lt;/p&gt;&lt;/body&gt;&lt;/html&gt;</translation>
    </message>
    <message>
        <source>Network</source>
        <translation>网络协议</translation>
    </message>
    <message>
        <source>Obfuscation</source>
        <translation>混淆</translation>
    </message>
    <message>
        <source>Obfuscation Host</source>
        <translation>混淆主机</translation>
    </message>
    <message>
        <source>&lt;html&gt;&lt;head/&gt;&lt;body&gt;&lt;p&gt;Traffic shaping used by Snell v6. &quot;unsafe-raw&quot; disables padding and is faster but easier to fingerprint.&lt;/p&gt;&lt;/body&gt;&lt;/html&gt;</source>
        <translation>&lt;html&gt;&lt;head/&gt;&lt;body&gt;&lt;p&gt;Snell v6 使用了流量塑形。&quot;unsafe-raw&quot; (不安全原始数据)禁止填充且更快，但是更容易被指纹识别。&lt;/p&gt;&lt;/body&gt;&lt;/html&gt;</translation>
    </message>
    <message>
        <source>Mode</source>
        <translation>模式</translation>
    </message>
    <message>
        <source>&lt;html&gt;&lt;head/&gt;&lt;body&gt;&lt;p&gt;Keep idle connections to the server open and reuse them for later requests.&lt;/p&gt;&lt;/body&gt;&lt;/html&gt;</source>
        <translation>&lt;html&gt;&lt;head/&gt;&lt;body&gt;&lt;p&gt;保持与服务器的空闲连接开启状态，并把它们重复用于之后的请求。&lt;/p&gt;&lt;/body&gt;&lt;/html&gt;</translation>
    </message>
    <message>
        <source>Reuse connections</source>
        <translation>重用连接</translation>
    </message>
    <message>
        <source>Obfuscation Host is used only with http or tls obfuscation.</source>
        <translation>「混淆主机」仅配合 http 或 tls 混淆使用。</translation>
    </message>
</context>
<context>
    <name>EditSocks</name>
    <message>
        <source>Password</source>
        <translation>密码</translation>
    </message>
    <message>
        <source>Version</source>
        <translation>版本</translation>
    </message>
    <message>
        <source>Username</source>
        <translation>用户名</translation>
    </message>
    <message>
        <source>5</source>
        <translation></translation>
    </message>
    <message>
        <source>4</source>
        <translation></translation>
    </message>
</context>
<context>
    <name>EditTailScale</name>
    <message>
        <source>State directory</source>
        <translation>状态目录</translation>
    </message>
    <message>
        <source>Auth key</source>
        <translation>认证钥匙</translation>
    </message>
    <message>
        <source>Control URL</source>
        <translation>控制 URL</translation>
    </message>
    <message>
        <source>Hostname</source>
        <translation>主机名</translation>
    </message>
    <message>
        <source>Accept routes</source>
        <translation>接受路由</translation>
    </message>
    <message>
        <source>Ephemeral</source>
        <translation>Ephemeral (短暂)</translation>
    </message>
    <message>
        <source>Exit node allow lan access</source>
        <translation>出口节点允许LAN访问</translation>
    </message>
    <message>
        <source>Advertise exit node</source>
        <translation>通告出口节点</translation>
    </message>
    <message>
        <source>Exit node</source>
        <translation>出口节点</translation>
    </message>
    <message>
        <source>&lt;html&gt;&lt;head/&gt;&lt;body&gt;&lt;p&gt;comma seperated list of subnets&lt;/p&gt;&lt;/body&gt;&lt;/html&gt;</source>
        <translation>&lt;html&gt;&lt;head/&gt;&lt;body&gt;&lt;p&gt;子网的逗号间隔列表&lt;/p&gt;&lt;/body&gt;&lt;/html&gt;</translation>
    </message>
    <message>
        <source>Advertise routes</source>
        <translation>通告路由</translation>
    </message>
    <message>
        <source>&lt;html&gt;&lt;head/&gt;&lt;body&gt;&lt;p&gt;use tailscale dns as the remote dns&lt;/p&gt;&lt;/body&gt;&lt;/html&gt;</source>
        <translation>&lt;html&gt;&lt;head/&gt;&lt;body&gt;&lt;p&gt;使用 tailscale dns 作为远程 dns&lt;/p&gt;&lt;/body&gt;&lt;/html&gt;</translation>
    </message>
    <message>
        <source>Global DNS</source>
        <translation>全局 DNS</translation>
    </message>
</context>
<context>
    <name>EditTrojan</name>
    <message>
        <source>Password</source>
        <translation>密码</translation>
    </message>
</context>
<context>
    <name>EditTrustTunnel</name>
    <message>
        <source>Password</source>
        <translation>密码</translation>
    </message>
    <message>
        <source>Custom SNI</source>
        <translation>自定义SNI</translation>
    </message>
    <message>
        <source>Server name sent in the TLS ClientHello instead of the TLS server name, which is still used to verify the certificate; not compatible with ECH or REALITY</source>
        <translation>在 TLS ClientHello 中发送的服务器名称（代替原 TLS 服务器名称），而原服务器名称仍用于验证证书；不兼容 ECH 或 REALITY</translation>
    </message>
    <message>
        <source>optional, e.g. cdn.example.net</source>
        <translation>可选，如 cdn.example.net</translation>
    </message>
    <message>
        <source>Client Random</source>
        <translation>客户端随机</translation>
    </message>
    <message>
        <source>TLS ClientHello random prefix required by the server&apos;s client_random_prefix rule; not compatible with ECH or REALITY</source>
        <translation>服务器的 client_random_prefix 规则所需要的 TLS ClientHello 随机前缀；不兼容 ECH 或 REALITY</translation>
    </message>
    <message>
        <source>prefix[/mask] (hex), e.g. a0b0/f0f0</source>
        <translation>前缀[/掩码] (hex), 如 a0b0/f0f0</translation>
    </message>
    <message>
        <source>Health Check</source>
        <translation>健康度检查</translation>
    </message>
    <message>
        <source>Username</source>
        <translation>用户名</translation>
    </message>
    <message>
        <source>Congestion Control</source>
        <translation>拥塞控制</translation>
    </message>
    <message>
        <source>QUIC</source>
        <translation></translation>
    </message>
    <message>
        <source>Client Random Key</source>
        <translation>客户端随机密钥</translation>
    </message>
    <message>
        <source>Hex key of the server&apos;s client_random_auth_key rule; use instead of Client Random</source>
        <translation>服务器 client_random_auth_key 规则的十六进制密钥；代替“客户端随机”使用</translation>
    </message>
</context>
<context>
    <name>EditTuic</name>
    <message>
        <source>Heartbeat</source>
        <translation>心跳包</translation>
    </message>
    <message>
        <source>UDP Relay Mode</source>
        <translation>UDP 中继模式</translation>
    </message>
    <message>
        <source>Password</source>
        <translation>密码</translation>
    </message>
    <message>
        <source>Zero RTT Handshake</source>
        <translation>零 RTT 握手</translation>
    </message>
    <message>
        <source>UUID</source>
        <translation></translation>
    </message>
    <message>
        <source>Congestion Control</source>
        <translation>拥塞控制</translation>
    </message>
    <message>
        <source>UDP over Stream</source>
        <translation>基于流的 UDP</translation>
    </message>
</context>
<context>
    <name>EditVMess</name>
    <message>
        <source>Generate UUID</source>
        <translation>生成 UUID</translation>
    </message>
    <message>
        <source>Alter Id</source>
        <translation>Alter Id (备用ID)</translation>
    </message>
    <message>
        <source>Security</source>
        <translation>安全性</translation>
    </message>
    <message>
        <source>UUID</source>
        <translation></translation>
    </message>
    <message>
        <source>Packet Encoding</source>
        <translation>数据包编码</translation>
    </message>
</context>
<context>
    <name>EditVless</name>
    <message>
        <source>UUID</source>
        <translation></translation>
    </message>
    <message>
        <source>Packet Encoding</source>
        <translation>数据包编码</translation>
    </message>
    <message>
        <source>Flow</source>
        <translation>流</translation>
    </message>
</context>
<context>
    <name>EditWireguard</name>
    <message>
        <source>Private Key</source>
        <translation>私钥</translation>
    </message>
    <message>
        <source>Public Key</source>
        <translation>公钥</translation>
    </message>
    <message>
        <source>Pre Shared Key</source>
        <translation>预共享钥匙</translation>
    </message>
    <message>
        <source>Reserved</source>
        <translation>保留</translation>
    </message>
    <message>
        <source>&lt;html&gt;&lt;head/&gt;&lt;body&gt;&lt;p&gt;persistent_keepalive_interval in seconds, or an AmneziaWG range such as 22-30&lt;/p&gt;&lt;/body&gt;&lt;/html&gt;</source>
        <translation>&lt;html&gt;&lt;head/&gt;&lt;body&gt;&lt;p&gt;以秒为单位的 persistent_keepalive_interval (持久保活间隔)值，或一个 AmneziaWG 范围，如 22-30&lt;/p&gt;&lt;/body&gt;&lt;/html&gt;</translation>
    </message>
    <message>
        <source>&lt;html&gt;&lt;head/&gt;&lt;body&gt;&lt;p&gt;&lt;br/&gt;&lt;/p&gt;&lt;/body&gt;&lt;/html&gt;</source>
        <translation>&lt;html&gt;&lt;head/&gt;&lt;body&gt;&lt;p&gt;&lt;br/&gt;&lt;/p&gt;&lt;/body&gt;&lt;/html&gt;</translation>
    </message>
    <message>
        <source>Persistent Keepalive</source>
        <translation>持久保活</translation>
    </message>
    <message>
        <source>&lt;html&gt;&lt;head/&gt;&lt;body&gt;&lt;p&gt;comma seperated list of subnets&lt;/p&gt;&lt;/body&gt;&lt;/html&gt;</source>
        <translation>&lt;html&gt;&lt;head/&gt;&lt;body&gt;&lt;p&gt;子网的逗号间隔列表&lt;/p&gt;&lt;/body&gt;&lt;/html&gt;</translation>
    </message>
    <message>
        <source>Local Address</source>
        <translation>本地地址</translation>
    </message>
    <message>
        <source>MTU</source>
        <translation>MTU</translation>
    </message>
    <message>
        <source>1420</source>
        <translation></translation>
    </message>
    <message>
        <source>Workers</source>
        <translation>工作线程</translation>
    </message>
    <message>
        <source>Use System Interface</source>
        <translation>使用系统接口</translation>
    </message>
    <message>
        <source>Enable Amnezia</source>
        <translation>启用 Amnezia</translation>
    </message>
    <message>
        <source>Edit the AmneziaWG obfuscation parameters (junk packets, magic headers, signatures).</source>
        <translation>编辑 AmneziaWG 混淆参数（垃圾数据包、魔法标头、签名）。</translation>
    </message>
    <message>
        <source>Amnezia Options</source>
        <translation>Amnezia 选项</translation>
    </message>
    <message>
        <source>Generate a Cloudflare WARP account and fill the key, address and endpoint fields above.</source>
        <translation>生成一个 Cloudflare WARP 账户并填写上面的钥匙、地址以及端点字段。</translation>
    </message>
    <message>
        <source>Generate Warp Config</source>
        <translation>生成 Warp 配置</translation>
    </message>
    <message>
        <source>Generating config...</source>
        <translation>正在生成配置…</translation>
    </message>
    <message>
        <source>Failed to generate warp config</source>
        <translation>生成 warp 配置失败</translation>
    </message>
    <message>
        <source>Success!</source>
        <translation>成功!</translation>
    </message>
    <message>
        <source>Open Amnezia Options and fix %1: %2</source>
        <translation>打开 Amnezia 选项并修复 %1: %2</translation>
    </message>
</context>
<context>
    <name>EditWireguardAmnezia</name>
    <message>
        <source>Amnezia Options</source>
        <translation>Amnezia 选项</translation>
    </message>
    <message>
        <source>Junk Packet Count (Jc)</source>
        <translation>垃圾数据包数量 (Jc)</translation>
    </message>
    <message>
        <source>Junk Packet Min Size (Jmin)</source>
        <translation>垃圾数据包最小大小 (Jmin)</translation>
    </message>
    <message>
        <source>Junk Packet Max Size (Jmax)</source>
        <translation>垃圾数据包最大大小 (Jmax)</translation>
    </message>
    <message>
        <source>Init Packet Junk Size (S1)</source>
        <translation>初始数据包垃圾大小 (S1)</translation>
    </message>
    <message>
        <source>Response Packet Junk Size (S2)</source>
        <translation>响应数据包垃圾大小 (S2)</translation>
    </message>
    <message>
        <source>Cookie Reply Packet Junk Size (S3)</source>
        <translation>Cookie 回复数据包垃圾大小 (S3)</translation>
    </message>
    <message>
        <source>Transport Packet Junk Size (S4)</source>
        <translation>传送数据包垃圾大小 (S4)</translation>
    </message>
    <message>
        <source>Init Packet Magic Header (H1)</source>
        <translation>初始数据包魔法标头 (H1)</translation>
    </message>
    <message>
        <source>Response Packet Magic Header (H2)</source>
        <translation>响应数据包魔术标头 (H2)</translation>
    </message>
    <message>
        <source>Cookie Reply Packet Magic Header (H3)</source>
        <translation>Cookie 回复数据包魔术标头 (H3)</translation>
    </message>
    <message>
        <source>Transport Packet Magic Header (H4)</source>
        <translation>传输数据包魔法标头 (H4)</translation>
    </message>
    <message>
        <source>Signature Packet (I1)</source>
        <translation>签名数据包 (I1)</translation>
    </message>
    <message>
        <source>Signature Packet (I2)</source>
        <translation>签名数据包 (I2)</translation>
    </message>
    <message>
        <source>Signature Packet (I3)</source>
        <translation>签名数据包 (I3)</translation>
    </message>
    <message>
        <source>Signature Packet (I4)</source>
        <translation>签名数据包 (I4)</translation>
    </message>
    <message>
        <source>Signature Packet (I5)</source>
        <translation>签名数据包 (I5)</translation>
    </message>
    <message>
        <source>&lt;html&gt;&lt;head/&gt;&lt;body&gt;&lt;p&gt;Fills the signature packets with the first QUIC packets Chrome would send to the given domain, so the handshake starts like an HTTP/3 connection to it. Generated locally; nothing is sent. Replaces I1-I5.&lt;/p&gt;&lt;/body&gt;&lt;/html&gt;</source>
        <translation>&lt;html&gt;&lt;head/&gt;&lt;body&gt;&lt;p&gt;用 Chrome 将发送到给定域的第一个 QUIC 包来填充签名数据包，因此握手就类似以 HTTP/3 连接到该域一样开始。本地生成；不会发送任何内容。替换 I1-I5。&lt;/p&gt;&lt;/body&gt;&lt;/html&gt;</translation>
    </message>
    <message>
        <source>Generate QUIC Signature...</source>
        <translation>生成 QUIC 签名...</translation>
    </message>
    <message>
        <source>&lt;html&gt;&lt;head/&gt;&lt;body&gt;&lt;p&gt;AmneziaWG 3.0. Base64 key. Requires S1-S4 to be at least 12.&lt;/p&gt;&lt;/body&gt;&lt;/html&gt;</source>
        <translation>&lt;html&gt;&lt;head/&gt;&lt;body&gt;&lt;p&gt;AmneziaWG 3.0。 Base64 钥匙。要求 S1-S4 至少 12。&lt;/p&gt;&lt;/body&gt;&lt;/html&gt;</translation>
    </message>
    <message>
        <source>Header Protection Key</source>
        <translation>标头保护钥匙</translation>
    </message>
    <message>
        <source>&lt;html&gt;&lt;head/&gt;&lt;body&gt;&lt;p&gt;AmneziaWG 3.0. Bytes of random trailing padding, as a value or range (e.g. 0-64).&lt;/p&gt;&lt;/body&gt;&lt;/html&gt;</source>
        <translation>&lt;html&gt;&lt;head/&gt;&lt;body&gt;&lt;p&gt;AmneziaWG 3.0。 随机尾部填充的字节数，可以为一个值或范围 (如 0-64)。&lt;/p&gt;&lt;/body&gt;&lt;/html&gt;</translation>
    </message>
    <message>
        <source>Content Padding Addition</source>
        <translation>内容填充附加量</translation>
    </message>
    <message>
        <source>&lt;html&gt;&lt;head/&gt;&lt;body&gt;&lt;p&gt;AmneziaWG 3.0. Seconds before a rekey is attempted, as a value or range.&lt;/p&gt;&lt;/body&gt;&lt;/html&gt;</source>
        <translation>&lt;html&gt;&lt;head/&gt;&lt;body&gt;&lt;p&gt;AmneziaWG 3.0. 尝试 rekey (重新密钥协商)前的秒数，可以是一个具体值或范围。&lt;/p&gt;&lt;/body&gt;&lt;/html&gt;</translation>
    </message>
    <message>
        <source>Rekey After Time</source>
        <translation>指定时间后 Rekey</translation>
    </message>
    <message>
        <source>&lt;html&gt;&lt;head/&gt;&lt;body&gt;&lt;p&gt;AmneziaWG 3.0. Seconds before a handshake is retried, as a value or range.&lt;/p&gt;&lt;/body&gt;&lt;/html&gt;</source>
        <translation>&lt;html&gt;&lt;head/&gt;&lt;body&gt;&lt;p&gt;AmneziaWG 3.0。 重试握手前的秒数，可以是一个具体值或范围。&lt;/p&gt;&lt;/body&gt;&lt;/html&gt;</translation>
    </message>
    <message>
        <source>Rekey Timeout</source>
        <translation>Rekey 超时</translation>
    </message>
    <message>
        <source>&lt;html&gt;&lt;head/&gt;&lt;body&gt;&lt;p&gt;AmneziaWG 3.0. Seconds before keys are rejected, as a value or range.&lt;/p&gt;&lt;/body&gt;&lt;/html&gt;</source>
        <translation>&lt;html&gt;&lt;head/&gt;&lt;body&gt;&lt;p&gt;AmneziaWG 3.0。 拒绝钥匙前的秒数，可以是一个具体值或范围。&lt;/p&gt;&lt;/body&gt;&lt;/html&gt;</translation>
    </message>
    <message>
        <source>Reject After Time</source>
        <translation>指定时间后拒绝</translation>
    </message>
    <message>
        <source>&lt;html&gt;&lt;head/&gt;&lt;body&gt;&lt;p&gt;AmneziaWG 3.0. Seconds of idle time before a keepalive is sent, as a value or range.&lt;/p&gt;&lt;/body&gt;&lt;/html&gt;</source>
        <translation>&lt;html&gt;&lt;head/&gt;&lt;body&gt;&lt;p&gt;AmneziaWG 3.0。 发送 keepalive (保活)前的空闲时间（秒数），可以是一个具体值或范围。&lt;/p&gt;&lt;/body&gt;&lt;/html&gt;</translation>
    </message>
    <message>
        <source>Keepalive Timeout</source>
        <translation>保活超时</translation>
    </message>
    <message>
        <source>&lt;html&gt;&lt;head/&gt;&lt;body&gt;&lt;p&gt;AmneziaWG 3.0. Handshake retries before giving up, as a value or range.&lt;/p&gt;&lt;/body&gt;&lt;/html&gt;</source>
        <translation>&lt;html&gt;&lt;head/&gt;&lt;body&gt;&lt;p&gt;AmneziaWG 3.0。 放弃前的握手重试次数，可以是一个具体值或范围。&lt;/p&gt;&lt;/body&gt;&lt;/html&gt;</translation>
    </message>
    <message>
        <source>Max Handshake Attempts</source>
        <translation>最多握手尝试次数</translation>
    </message>
    <message>
        <source>&lt;html&gt;&lt;head/&gt;&lt;body&gt;&lt;p&gt;AmneziaWG 3.1. Appends a random trailer to every packet, so message types are no longer identifiable by size.&lt;/p&gt;&lt;/body&gt;&lt;/html&gt;</source>
        <translation>&lt;html&gt;&lt;head/&gt;&lt;body&gt;&lt;p&gt;AmneziaWG 3.1。附加一个随机的尾部到每个数据包，这样消息类型就不再能够通过大小识别。&lt;/p&gt;&lt;/body&gt;&lt;/html&gt;</translation>
    </message>
    <message>
        <source>Random Trailers</source>
        <translation>随机尾部</translation>
    </message>
    <message>
        <source>&lt;html&gt;&lt;head/&gt;&lt;body&gt;&lt;p&gt;AmneziaWG 3.1. Never send cookie replies, so an under-load peer stays silent instead of answering with a fixed-size message.&lt;/p&gt;&lt;/body&gt;&lt;/html&gt;</source>
        <translation>&lt;html&gt;&lt;head/&gt;&lt;body&gt;&lt;p&gt;AmneziaWG 3.1。永不发送 cookie 回复，所以负载过低的对端节点会保持静默而不是用固定大小的消息回答。&lt;/p&gt;&lt;/body&gt;&lt;/html&gt;</translation>
    </message>
    <message>
        <source>Disable Cookies</source>
        <translation>禁用 Cookies</translation>
    </message>
    <message>
        <source>QUIC Signature</source>
        <translation>QUIC 签名</translation>
    </message>
    <message>
        <source>Domain to imitate:</source>
        <translation>要模仿的域:</translation>
    </message>
    <message>
        <source>Generating...</source>
        <translation>正在生成...</translation>
    </message>
    <message>
        <source>Failed to reach the core.</source>
        <translation>连接到核心失败。</translation>
    </message>
    <message>
        <source>Failed to generate QUIC signature</source>
        <translation>生成 QUIC 签名失败</translation>
    </message>
</context>
<context>
    <name>EditXrayVless</name>
    <message>
        <source>UUID</source>
        <translation>UUID</translation>
    </message>
    <message>
        <source>Encryption</source>
        <translation>加密</translation>
    </message>
    <message>
        <source>Flow</source>
        <translation>流</translation>
    </message>
</context>
<context>
    <name>GlobalHotkeys</name>
    <message>
        <source>This key can&apos;t be used in a global hotkey.</source>
        <translation>这个键不能在全局热键中使用。</translation>
    </message>
    <message>
        <source>Another application or a system shortcut already uses it.</source>
        <translation>另一个应用程序或系统快捷键已在使用它。</translation>
    </message>
    <message>
        <source>Global hotkeys are not supported in this desktop session.</source>
        <translation>在这个桌面会话中不支持全局热键。</translation>
    </message>
    <message>
        <source>It needs Ctrl, Alt or Meta, otherwise it would block typing in other applications.</source>
        <translation>它需要 Ctrl, Alt 或 Meta，否则它将会在其它应用程序中阻止键入。</translation>
    </message>
    <message>
        <source>&quot;%1&quot; already uses it.</source>
        <translation>&quot;%1&quot; 已在使用它。</translation>
    </message>
    <message>
        <source>KDE&apos;s shortcut service failed: %1</source>
        <translation>KDE 的快捷方式服务失败: %1</translation>
    </message>
    <message>
        <source>Registration failed: X error %1</source>
        <translation>注册失败: X 错误 %1</translation>
    </message>
    <message>
        <source>Registration failed: %1</source>
        <translation>注册失败: %1</translation>
    </message>
</context>
<context>
    <name>GroupItem</name>
    <message>
        <source>Update Subscription</source>
        <translation>更新订阅</translation>
    </message>
    <message>
        <source>Edit</source>
        <translation>编辑</translation>
    </message>
    <message>
        <source>Remove</source>
        <translation>删除</translation>
    </message>
    <message>
        <source>Basic</source>
        <translation>基本</translation>
    </message>
    <message>
        <source>Subscription</source>
        <translation>订阅</translation>
    </message>
    <message>
        <source>Archive</source>
        <translation>档案</translation>
    </message>
    <message>
        <source>Last update: %1</source>
        <translation>最后更新 %1</translation>
    </message>
    <message>
        <source>This group was deleted while you were editing it.</source>
        <translation>这个分组在你编辑时被删除。</translation>
    </message>
    <message>
        <source>Confirmation</source>
        <translation>确认</translation>
    </message>
    <message>
        <source>Remove %1?</source>
        <translation>删除 %1 吗?</translation>
    </message>
</context>
<context>
    <name>GuiStartup</name>
    <message>
        <source>A background Throne (PID %1) is running on this computer (data directory: %2). Tun mode is unavailable here while it runs; stop it or use thronectl.</source>
        <translation></translation>
    </message>
    <message>
        <source>Throne is running in the background</source>
        <translation>Throne 正在后台运行</translation>
    </message>
</context>
<context>
    <name>IpEntriesModel</name>
    <message>
        <source>%1 ms</source>
        <translation>%1 毫秒</translation>
    </message>
    <message>
        <source>The longest query name the resolver relays: max_qname_len for DNSTT, lenLimit for XDNS.</source>
        <translation>该解析器转发的最长查询名称：DNSTT 使用 max_qname_len，XDNS 使用 lenLimit。</translation>
    </message>
    <message>
        <source>The largest payload per query the resolver relays.</source>
        <translation>该 DNS 解析器转发的每条查询的最大有效载荷。</translation>
    </message>
    <message>
        <source>The largest payload per answer the resolver relays.</source>
        <translation>该 DNS 解析器转发的每条响应的最大有效载荷。</translation>
    </message>
    <message>
        <source>Address</source>
        <translation>地址</translation>
    </message>
    <message>
        <source>Port</source>
        <translation>端口</translation>
    </message>
    <message>
        <source>Latency</source>
        <translation>延迟</translation>
    </message>
    <message>
        <source>Max QNAME</source>
        <translation>最大 QNAME</translation>
    </message>
    <message>
        <source>Upload MTU</source>
        <translation>上传 MTU</translation>
    </message>
    <message>
        <source>Download MTU</source>
        <translation>下载 MTU</translation>
    </message>
</context>
<context>
    <name>IpListItem</name>
    <message>
        <source>%1 d</source>
        <translation>%1 天</translation>
    </message>
    <message>
        <source>%1 h</source>
        <translation>%1 小时</translation>
    </message>
    <message>
        <source>%1 min</source>
        <translation>%1 分</translation>
    </message>
    <message>
        <source>updated just now</source>
        <translation>刚更新</translation>
    </message>
    <message>
        <source>updated %1 min ago</source>
        <translation>%1 分钟前更新</translation>
    </message>
    <message>
        <source>updated %1 h ago</source>
        <translation>%1 小时前更新</translation>
    </message>
    <message>
        <source>updated %1 d ago</source>
        <translation>%1 天前更新</translation>
    </message>
    <message>
        <source>URL %1</source>
        <translation>URL %1</translation>
    </message>
    <message>
        <source>Rule-set %1</source>
        <translation>规则集 %1</translation>
    </message>
    <message>
        <source>Refreshing…</source>
        <translation>正在刷新…</translation>
    </message>
    <message>
        <source>Refresh now</source>
        <translation>立即刷新</translation>
    </message>
    <message>
        <source>Delete</source>
        <translation>删除</translation>
    </message>
    <message>
        <source>Edit</source>
        <translation>编辑</translation>
    </message>
    <message>
        <source>View</source>
        <translation>查看</translation>
    </message>
    <message numerus="yes">
        <source>%Ln entries</source>
        <translation>
            <numerusform>%Ln 项</numerusform>
        </translation>
    </message>
    <message>
        <source>Result of a deleted scan</source>
        <translation>已删除扫描的结果</translation>
    </message>
    <message>
        <source>Result of scan %1</source>
        <translation>扫描「%1」的结果</translation>
    </message>
    <message>
        <source>auto-update every %1</source>
        <translation>自动更新每隔 %1</translation>
    </message>
    <message>
        <source>never updated</source>
        <translation>从不更新</translation>
    </message>
    <message>
        <source>Last update failed: %1</source>
        <translation>上次更新失败: %1</translation>
    </message>
</context>
<context>
    <name>LiveChart</name>
    <message>
        <source>Down</source>
        <translation>下</translation>
    </message>
    <message>
        <source>Up</source>
        <translation>上</translation>
    </message>
    <message>
        <source>Waiting for traffic</source>
        <translation>正在等待流量</translation>
    </message>
    <message>
        <source>The graph fills in once a second while a profile is running.</source>
        <translation>当配置档在运行时，图表每秒填充一次。</translation>
    </message>
    <message>
        <source>Total</source>
        <translation>合计(Σ)</translation>
    </message>
    <message>
        <source>Stopped</source>
        <translation>已停止</translation>
    </message>
    <message>
        <source>All lines are hidden</source>
        <translation>隐藏所有线条</translation>
    </message>
    <message>
        <source>Click an outbound or a direction in the legend to show it.</source>
        <translation>点击图例中一个出站口或方向即可显示它。</translation>
    </message>
    <message>
        <source>peak of %1 s</source>
        <translation>%1 秒的峰值</translation>
    </message>
    <message>
        <source>Not running</source>
        <translation>未在运行</translation>
    </message>
    <message>
        <source>Started %1</source>
        <translation>已启动 %1</translation>
    </message>
    <message>
        <source>Switched to %1</source>
        <translation>已切换到 %1</translation>
    </message>
    <message>
        <source>Smooth lines</source>
        <translation>平滑线条</translation>
    </message>
    <message>
        <source>Profile events</source>
        <translation>配置档事件</translation>
    </message>
    <message>
        <source>Show all lines</source>
        <translation>显示所有线条</translation>
    </message>
    <message>
        <source>Clear history</source>
        <translation>清除历史</translation>
    </message>
</context>
<context>
    <name>MainWindow</name>
    <message>
        <source>Program</source>
        <translation>程序</translation>
    </message>
    <message>
        <source>Settings</source>
        <translation>设置</translation>
    </message>
    <message>
        <source>Groups</source>
        <translation>分组</translation>
    </message>
    <message>
        <source>Routing</source>
        <translation>路由</translation>
    </message>
    <message>
        <source>Tools</source>
        <translation>工具</translation>
    </message>
    <message>
        <source>Tun Mode</source>
        <translation>Tun 模式</translation>
    </message>
    <message>
        <source>System Proxy</source>
        <translation>系统代理</translation>
    </message>
    <message>
        <source>Logs</source>
        <translation>日志</translation>
    </message>
    <message>
        <source>Filter logs</source>
        <translation>过滤日志</translation>
    </message>
    <message>
        <source>Match case</source>
        <translation>匹配大小写</translation>
    </message>
    <message>
        <source>Regular expression</source>
        <translation>正则表达式</translation>
    </message>
    <message>
        <source>Connections</source>
        <translation>连接</translation>
    </message>
    <message>
        <source>Traffic Graph</source>
        <translation>流量图</translation>
    </message>
    <message>
        <source>Preferences</source>
        <translation>首选项</translation>
    </message>
    <message>
        <source>Server</source>
        <translation>服务器</translation>
    </message>
    <message>
        <source>Share</source>
        <translation>分享</translation>
    </message>
    <message>
        <source>Hidden menu</source>
        <translation>隐藏菜单</translation>
    </message>
    <message>
        <source>Operation Mode</source>
        <translation>操作模式</translation>
    </message>
    <message>
        <source>Routing Menu</source>
        <translation>路由菜单</translation>
    </message>
    <message>
        <source>Clean up</source>
        <translation>清理</translation>
    </message>
    <message>
        <source>Exit</source>
        <translation>退出</translation>
    </message>
    <message>
        <source>Show Window</source>
        <translation>显示窗口</translation>
    </message>
    <message>
        <source>Basic Settings</source>
        <translation>基本设置</translation>
    </message>
    <message>
        <source>New profile</source>
        <translation>新建配置档</translation>
    </message>
    <message>
        <source>Manage Groups</source>
        <translation>管理分组</translation>
    </message>
    <message>
        <source>Start</source>
        <translation>启动</translation>
    </message>
    <message>
        <source>Stop</source>
        <translation>停止</translation>
    </message>
    <message>
        <source>Routing Settings</source>
        <translation>路由设置</translation>
    </message>
    <message>
        <source>Add profile from clipboard</source>
        <translation>添加剪贴板中的配置档</translation>
    </message>
    <message>
        <source>Delete</source>
        <translation>删除</translation>
    </message>
    <message>
        <source>QR Code and link</source>
        <translation>二维码和链接</translation>
    </message>
    <message>
        <source>Clear Group test result</source>
        <translation>清除本组测试结果</translation>
    </message>
    <message>
        <source>Reset Traffic</source>
        <translation>重置流量</translation>
    </message>
    <message>
        <source>Scan QR Code</source>
        <translation>扫描二维码</translation>
    </message>
    <message>
        <source>Remove Duplicates</source>
        <translation>移除重复项</translation>
    </message>
    <message>
        <source>Start with system</source>
        <translation>随系统启动</translation>
    </message>
    <message>
        <source>Remember last profile</source>
        <translation>记住上次的配置档</translation>
    </message>
    <message>
        <source>Allow other devices to connect</source>
        <translation>允许其他设备连接</translation>
    </message>
    <message>
        <source>Remove Unavailable</source>
        <translation>移除不可用项</translation>
    </message>
    <message>
        <source>Select All</source>
        <translation>全选</translation>
    </message>
    <message>
        <source>Copy links of selected (Deep Links)</source>
        <translation>复制选定项的链接 (深度链接)</translation>
    </message>
    <message>
        <source>Copy links of selected</source>
        <translation>复制选定项的链接</translation>
    </message>
    <message>
        <source>Clone</source>
        <translation>克隆</translation>
    </message>
    <message>
        <source>Update subscription</source>
        <translation>更新订阅</translation>
    </message>
    <message>
        <source>Resolve Domain for group</source>
        <translation>为本组解析域名</translation>
    </message>
    <message>
        <source>Tun Settings</source>
        <translation>Tun 设置</translation>
    </message>
    <message>
        <source>Preset Settings</source>
        <translation>预设设置</translation>
    </message>
    <message>
        <source>OTP Manager</source>
        <translation>OTP 管理器</translation>
    </message>
    <message>
        <source>IP Scanner</source>
        <translation>IP 扫描器</translation>
    </message>
    <message>
        <source>Restart Program</source>
        <translation>重启程序</translation>
    </message>
    <message>
        <source>Open Config Folder</source>
        <translation>打开配置文件夹</translation>
    </message>
    <message>
        <source>Open Web dashboard</source>
        <translation>打开网络仪表盘</translation>
    </message>
    <message>
        <source>Restart Core</source>
        <translation>重启核心</translation>
    </message>
    <message>
        <source>Stop Testing</source>
        <translation>停止测试</translation>
    </message>
    <message>
        <source>Url Test Selected</source>
        <translation>URL 测试选定项</translation>
    </message>
    <message>
        <source>Url Test Group</source>
        <translation>URL 测试本组</translation>
    </message>
    <message>
        <source>Resolve Selected Domain</source>
        <translation>解析选定域名</translation>
    </message>
    <message>
        <source>Remove Invalid</source>
        <translation>移除无效项</translation>
    </message>
    <message>
        <source>Remove Insecure Configs</source>
        <translation>移除不安全的配置</translation>
    </message>
    <message>
        <source>Speedtest Current</source>
        <translation>速度测试当前项</translation>
    </message>
    <message>
        <source>Speedtest Selected</source>
        <translation>速度测试选定项</translation>
    </message>
    <message>
        <source>Speedtest Group</source>
        <translation>速度测试本组</translation>
    </message>
    <message>
        <source>Hide window</source>
        <translation>隐藏窗口</translation>
    </message>
    <message>
        <source>Add profile from File(s)</source>
        <translation>添加文件中的配置档</translation>
    </message>
    <message>
        <source>Resolve out IP for group</source>
        <translation>为本组解析 IP</translation>
    </message>
    <message>
        <source>Add new Group</source>
        <translation>添加新分组</translation>
    </message>
    <message>
        <source>Edit current Group</source>
        <translation>编辑当前分组</translation>
    </message>
    <message>
        <source>Delete current Group</source>
        <translation>删除当前分组</translation>
    </message>
    <message>
        <source>Update all subscriptions</source>
        <translation>更新所有订阅</translation>
    </message>
    <message>
        <source>Resolve Selected Out IP</source>
        <translation>解析选定的出口 IP</translation>
    </message>
    <message>
        <source>Refresh Column Widths</source>
        <translation>刷新列宽</translation>
    </message>
    <message>
        <source>Copy Test Result</source>
        <translation>复制测试结果</translation>
    </message>
    <message>
        <source>Clear Test Result</source>
        <translation>清理测试结果</translation>
    </message>
    <message>
        <source>About</source>
        <translation>关于</translation>
    </message>
    <message>
        <source>Update Rule-Sets</source>
        <translation>更新规则集</translation>
    </message>
    <message>
        <source>Runtime Stats</source>
        <translation>运行时统计</translation>
    </message>
    <message>
        <source>Export config</source>
        <translation>导出配置</translation>
    </message>
    <message>
        <source>Integration Settings</source>
        <translation>集成设置</translation>
    </message>
    <message>
        <source>Auto Selector Stats</source>
        <translation>自动选择器统计</translation>
    </message>
    <message>
        <source>Traffic Stats</source>
        <translation>流量统计</translation>
    </message>
    <message>
        <source>Check For Update</source>
        <translation>检查更新</translation>
    </message>
    <message>
        <source>[Auto selector] Reusing existing test results; ranked %1 profiles.</source>
        <translation>[自动选择器] 正在重用现有测试结果； 已排名 %1 个配置档。</translation>
    </message>
    <message>
        <source>[Auto selector] Measuring %1 not-yet-tested profiles...</source>
        <translation>[自动选择器] 正在测量 %1 个尚未测试的配置档...</translation>
    </message>
    <message>
        <source>[Auto selector] %1 working profiles found; starting now while the remaining %2 are still being measured.</source>
        <translation>[自动选择器] 找到 %1 个有效配置档；立即开始，其余 %2 个仍在测量中。</translation>
    </message>
    <message>
        <source>[Auto selector] Ranked %1 profiles.</source>
        <translation>[自动选择器] 已排名 %1 个配置档。</translation>
    </message>
    <message>
        <source>[Auto selector] Every profile is measured and the best ones changed — rebuilding with the full ranking.</source>
        <translation>[自动选择器] 每个配置档都被测量，并更改最好的 — 正在用完整排名重建。</translation>
    </message>
    <message>
        <source>[Auto selector] The subscription replaced profiles it was running on — rebuilding.</source>
        <translation>[自动选择器] 订阅替换了它正在运行的配置档 - 正在重建。</translation>
    </message>
    <message>
        <source>[Auto selector] Every running profile stopped working — rebuilding from the next best candidates.</source>
        <translation>[自动选择器] 所有正在运行的配置档都停止运作 - 正在从下一个最好的候选配置项重新构建。</translation>
    </message>
    <message>
        <source>Enable Filter</source>
        <translation>启用筛选</translation>
    </message>
    <message>
        <source>Close every connection listed below</source>
        <translation>关闭下面列出的每个连接</translation>
    </message>
    <message>
        <source>Total</source>
        <translation>合计(Σ)</translation>
    </message>
    <message>
        <source>Downloaded</source>
        <translation>下载(↓)</translation>
    </message>
    <message>
        <source>Uploaded</source>
        <translation>上传(↑)</translation>
    </message>
    <message>
        <source>Download Speed</source>
        <translation>下载速度(↓)</translation>
    </message>
    <message>
        <source>Upload Speed</source>
        <translation>上传速度(↑)</translation>
    </message>
    <message>
        <source>Sort By:</source>
        <translation>排序依据:</translation>
    </message>
    <message>
        <source>Failed to close connections: %1</source>
        <translation>关闭连接失败: %1</translation>
    </message>
    <message>
        <source>IPC error</source>
        <translation>IPC 错误</translation>
    </message>
    <message>
        <source>Collapse All</source>
        <translation>折叠全部</translation>
    </message>
    <message>
        <source>Expand All</source>
        <translation>展开全部</translation>
    </message>
    <message>
        <source>No active routing profile found.</source>
        <translation>找不到活动的路由配置档。</translation>
    </message>
    <message>
        <source>The current routing profile is locked against modifications.</source>
        <translation>当前路由配置档被锁定不得修改。</translation>
    </message>
    <message>
        <source>The current routing profile is raw JSON.</source>
        <translation>当前路由配置档是原始的 JSON。</translation>
    </message>
    <message>
        <source>The current routing profile auto-updates from a URL.</source>
        <translation>当前路由配置档从一个 URL 自动更新。</translation>
    </message>
    <message>
        <source>The current routing profile is updated by the subscription &quot;%1&quot;.</source>
        <translation>当前路由配置档由订阅 &quot;%1&quot; 更新。</translation>
    </message>
    <message>
        <source>Removed %1 from the %2 rules of &quot;%3&quot;</source>
        <translation>已从 “%3” 的 %2 条规则中移除 %1 条</translation>
    </message>
    <message>
        <source>Failed to add routing rule: %1</source>
        <translation>添加路由规则失败: %1</translation>
    </message>
    <message>
        <source>Moved %1 from the %2 to the %3 rules of &quot;%4&quot;</source>
        <translation>已从 %2 移动 %1 条到 &apos;%4&apos; 的 %3 条规则</translation>
    </message>
    <message>
        <source>Failed to save routing rule: %1</source>
        <translation>保存路由规则失败: %1</translation>
    </message>
    <message>
        <source>Warp-bypass</source>
        <translation>Warp-bypass</translation>
    </message>
    <message>
        <source>Modify rules in &quot;%1&quot;</source>
        <translation>修改 &quot;%1&quot; 中的规则</translation>
    </message>
    <message>
        <source>Modify rules</source>
        <translation>修改规则</translation>
    </message>
    <message>
        <source>Specific server</source>
        <translation>特定服务器</translation>
    </message>
    <message>
        <source>Route through another server</source>
        <translation>通过另一个服务器路由</translation>
    </message>
    <message>
        <source>There are no servers to route to.</source>
        <translation>没有路由到的服务器。</translation>
    </message>
    <message>
        <source>%1 rules</source>
        <translation>%1 条规则</translation>
    </message>
    <message>
        <source>Specific server rules</source>
        <translation>特定服务器规则</translation>
    </message>
    <message>
        <source>Routing profile &quot;%1&quot;. Checked targets go %2; unchecking one removes its rule.</source>
        <translation>路由配置档“%1”。已勾选目标走 %2；取消勾选的会移除它的规则。</translation>
    </message>
    <message>
        <source>Routing profile &quot;%1&quot;. Checked targets go through the chosen server; unchecking one removes its rule.</source>
        <translation>路由配置档 &quot;%1&quot;。勾选的目标会走选择的服务器；取消勾选则会移除它的规则。</translation>
    </message>
    <message>
        <source>Server (type part of its name to search):</source>
        <translation>服务器 (输入其名称的部分进行搜索):</translation>
    </message>
    <message>
        <source>Search servers</source>
        <translation>搜索服务器</translation>
    </message>
    <message>
        <source>in %1</source>
        <translation>于 %1</translation>
    </message>
    <message>
        <source>Checking it moves the rule from %1 to %2</source>
        <translation>勾选它会把规则从 %1 移到 %2</translation>
    </message>
    <message>
        <source>covered by %1 in %2</source>
        <translation>由 %1 覆盖于 %2</translation>
    </message>
    <message>
        <source>%1 in %2 is matched first, so this rule would never apply</source>
        <translation>%2 中的 %1 会首先被匹配，因此这条规则永不应用</translation>
    </message>
    <message>
        <source>%1  (%2)</source>
        <translation>%1  (%2)</translation>
    </message>
    <message>
        <source>Added %1</source>
        <translation>增加 %1 项</translation>
    </message>
    <message>
        <source>Moved %1</source>
        <translation>移动 %1 项</translation>
    </message>
    <message>
        <source>Removed %1</source>
        <translation>移除 %1 项</translation>
    </message>
    <message>
        <source>%1 rules:
%2</source>
        <translation>%1 条规则:
%2</translation>
    </message>
    <message>
        <source>Process %1</source>
        <translation>进程 %1</translation>
    </message>
    <message>
        <source>Appended %1 to the %2 rules of &quot;%3&quot;</source>
        <translation>追加 %1 到 &quot;%3&quot; 的 %2 条规则</translation>
    </message>
    <message>
        <source>Direct</source>
        <translation>直连</translation>
    </message>
    <message>
        <source>Proxy</source>
        <translation>代理</translation>
    </message>
    <message>
        <source>Block</source>
        <translation>阻止</translation>
    </message>
    <message>
        <source>Copied: %1</source>
        <translation>已复制: %1</translation>
    </message>
    <message>
        <source>Copy Destination (%1)</source>
        <translation>复制目标 (%1)</translation>
    </message>
    <message>
        <source>Copy Process Name (%1)</source>
        <translation>复制进程名称 (%1)</translation>
    </message>
    <message>
        <source>Close all connections (%1)</source>
        <translation>关闭所有连接 (%1)</translation>
    </message>
    <message>
        <source>Close connection</source>
        <translation>关闭连接</translation>
    </message>
    <message>
        <source>Copy Process Name</source>
        <translation>复制进程名称</translation>
    </message>
    <message>
        <source>Close all connections for &quot;%1&quot; (%2)</source>
        <translation>关闭 &quot;%1&quot; 的所有连接 (%2)</translation>
    </message>
    <message>
        <source>%1: cannot be opened</source>
        <translation>%1: 无法打开</translation>
    </message>
    <message>
        <source>%1: larger than 50 MB, skipped</source>
        <translation>%1: 大于 50 MB，已跳过</translation>
    </message>
    <message>
        <source>%1: no QR code found</source>
        <translation>%1: 找不到二维码</translation>
    </message>
    <message>
        <source>%1: not a readable config file</source>
        <translation>%1: 不是一个可读取的配置文件</translation>
    </message>
    <message>
        <source>Nothing could be imported:</source>
        <translation>没有可被导入的内容:</translation>
    </message>
    <message>
        <source>Ignored deeplink with unknown command: %1</source>
        <translation>忽略了带未知命令的深度链接: %1</translation>
    </message>
    <message>
        <source>Import routing profile</source>
        <translation>导入路由配置档</translation>
    </message>
    <message>
        <source>The link could not be parsed:
</source>
        <translation>该链接无法解析:
</translation>
    </message>
    <message>
        <source>Imported profile</source>
        <translation>已导入配置档</translation>
    </message>
    <message>
        <source>Add this routing profile?
Name: %1</source>
        <translation>添加这个路由配置档吗?
名称: %1</translation>
    </message>
    <message>
        <source>Note:</source>
        <translation>备注:</translation>
    </message>
    <message>
        <source>Add remote routing profiles</source>
        <translation>添加远程路由配置档</translation>
    </message>
    <message>
        <source>The link did not contain any valid remote routing profiles.</source>
        <translation>该链接不包含任何有效的远程路由配置档。</translation>
    </message>
    <message>
        <source>Add these remote routing profiles?</source>
        <translation>添加这些远程路由配置档吗?</translation>
    </message>
    <message>
        <source>Auto update</source>
        <translation>自动更新</translation>
    </message>
    <message>
        <source>Add subscription</source>
        <translation>添加订阅</translation>
    </message>
    <message>
        <source>The link did not contain a subscription URL.</source>
        <translation>该链接不包含订阅 URL。</translation>
    </message>
    <message>
        <source>Add this subscription?
Name: %1
URL: %2</source>
        <translation>添加这个订阅吗?
名称: %1
URL: %2</translation>
    </message>
    <message>
        <source>Tun Settings changed</source>
        <translation>Tun 设置已改变</translation>
    </message>
    <message>
        <source>Restart Tun to take effect.</source>
        <translation>重启 Tun 以生效。</translation>
    </message>
    <message>
        <source>Confirmation</source>
        <translation>确认</translation>
    </message>
    <message>
        <source>Settings changed</source>
        <translation>设置已改变</translation>
    </message>
    <message>
        <source>Restart the program to take effect.</source>
        <translation>重启程序以生效。</translation>
    </message>
    <message>
        <source>Profile</source>
        <translation>配置档</translation>
    </message>
    <message>
        <source>Imported %1 profile(s)</source>
        <translation>导入了 %1 个配置档</translation>
    </message>
    <message>
        <source>Provider: %1</source>
        <translation>提供方: %1</translation>
    </message>
    <message>
        <source>Type: Subscription</source>
        <translation>类型: 订阅</translation>
    </message>
    <message>
        <source>Last updated</source>
        <translation>上次更新</translation>
    </message>
    <message>
        <source>Auto-update: every %1</source>
        <translation>自动更新: 每隔 %1</translation>
    </message>
    <message>
        <source>Used</source>
        <translation>已用</translation>
    </message>
    <message>
        <source>Remaining</source>
        <translation>剩余</translation>
    </message>
    <message>
        <source>Expires</source>
        <translation>过期</translation>
    </message>
    <message>
        <source>Support</source>
        <translation>支持</translation>
    </message>
    <message>
        <source>Portal</source>
        <translation>门户</translation>
    </message>
    <message>
        <source>Announcement</source>
        <translation>通告</translation>
    </message>
    <message>
        <source>Error</source>
        <translation>错误</translation>
    </message>
    <message>
        <source>Edit selected Group</source>
        <translation>编辑选定分组</translation>
    </message>
    <message>
        <source>This group was deleted while you were editing it.</source>
        <translation>这个分组在你编辑时被删除。</translation>
    </message>
    <message>
        <source>Delete selected Group</source>
        <translation>删除选定分组</translation>
    </message>
    <message>
        <source>Remove %1?</source>
        <translation>删除 %1 吗?</translation>
    </message>
    <message>
        <source>Url Test selected Group</source>
        <translation>URL 测试选定分组</translation>
    </message>
    <message>
        <source>Speed Test selected Group</source>
        <translation>速度测试选定分组</translation>
    </message>
    <message>
        <source>New logs are held while the view is scrolled up</source>
        <translation>向上滚动视图时，新日志会被保留</translation>
    </message>
    <message>
        <source>Disable Filter</source>
        <translation>禁用筛选</translation>
    </message>
    <message>
        <source>Jump to latest</source>
        <translation>跳到最新项</translation>
    </message>
    <message>
        <source>Jump to latest (%1 new)</source>
        <translation>跳到最新项 (%1 条新消息)</translation>
    </message>
    <message>
        <source>Clear</source>
        <translation>清除</translation>
    </message>
    <message>
        <source>a required category</source>
        <translation>一个必需的类别</translation>
    </message>
    <message>
        <source>The Xray config &quot;%1&quot; needs &quot;%2&quot;, but the installed %3 does not contain it.</source>
        <translation>Xray 配置 &quot;%1&quot; 需要 &quot;%2&quot;，但已安装的 %3 不包含它。</translation>
    </message>
    <message>
        <source>The Xray config &quot;%1&quot; needs geo data files that are not installed: %2. Download them under Basic Settings &gt; Core &gt; Xray Core.</source>
        <translation>Xray 配置 “%1” 需要未安装的地理数据文件：%2。在 “基本设置 &gt; 核心 &gt; Xray 核心” 部分下载它们。</translation>
    </message>
    <message>
        <source>Geo asset files are already being downloaded.</source>
        <translation>地理资源文件已在下载中。</translation>
    </message>
    <message>
        <source>Geo asset missing category</source>
        <translation>Geo 资源缺失类别</translation>
    </message>
    <message>
        <source>The Xray config &quot;%1&quot; needs &quot;%2&quot;, but the installed %3 does not contain it.
Re-downloading from the same source will not fix this — the data file does not include that category. Set the GeoIP/GeoSite asset URL in Settings to a source that provides &quot;%2&quot;, then delete %3 from the app folder and download it again.</source>
        <translation>Xray 配置 &quot;%1&quot; 需要 &quot;%2&quot;, 但安装的 %3 不包含它。
从同一来源重新下载无法修复这个问题 — 数据文件不包含该列表。在「设置」中设置 GeoIP/GeoSite 资源 URL 为一个提供 &quot;%2&quot; 的来源，然后从应用文件夹中删除 %3 并重新下载它。</translation>
    </message>
    <message>
        <source>Geo asset files required</source>
        <translation>需要 Geo 资源文件</translation>
    </message>
    <message>
        <source>The Xray config &quot;%1&quot; uses geoip/geosite routing rules, but the required data files (geoip.dat / geosite.dat) are not installed.
Download them now?</source>
        <translation>Xray 配置 &quot;%1&quot; 使用了 geoip/geosite 路由规则，但未安装所需的数据文件 (geoip.dat / geosite.dat)。
立即安装它们吗?</translation>
    </message>
    <message>
        <source>Geo asset download failed</source>
        <translation>Geo 资源下载失败</translation>
    </message>
    <message>
        <source>Downloaded Xray geo asset files.</source>
        <translation>已下载的 Xray geo 资源文件。</translation>
    </message>
    <message>
        <source>Geo assets installed</source>
        <translation>Geo 资源已安装</translation>
    </message>
    <message>
        <source>Geo data files were downloaded successfully.
Please try again.</source>
        <translation>成功下载过 Geo 数据文件。
请再试一次。</translation>
    </message>
    <message>
        <source>BuildConfig return error</source>
        <translation>BuildConfig 返回错误</translation>
    </message>
    <message>
        <source>Kill switch is blocking traffic</source>
        <translation>急停开关正在拦截流量</translation>
    </message>
    <message>
        <source>Kill switch is not active</source>
        <translation>急停开关未激活</translation>
    </message>
    <message>
        <source>Restart as Administrator</source>
        <translation>以管理员身份重启</translation>
    </message>
    <message>
        <source>Retry</source>
        <translation>重试</translation>
    </message>
    <message>
        <source>Grant Privileges</source>
        <translation>赋予特权</translation>
    </message>
    <message>
        <source>Disable Kill Switch</source>
        <translation>禁用急停开关</translation>
    </message>
    <message>
        <source>Kill switch</source>
        <translation>急停开关</translation>
    </message>
    <message>
        <source>The kill switch could not be updated to allow this profile&apos;s extra core, so the profile was not started. See the log for details.</source>
        <translation>急停开关无法更新以允许这个配置档的附加核心，因此该配置档未启动。详情见日志。</translation>
    </message>
    <message>
        <source>Strict routing unavailable</source>
        <translation>严格的路由不可用</translation>
    </message>
    <message>
        <source>Windows could not enable strict routing. Open Tun Settings, disable Strict Route, and start the profile again.
Disabling Strict Route may cause DNS leaks.
Error: %1</source>
        <translation>Windows 无法启用严格路由。打开 Tun 设置，禁用“严格路由”，并再次启动该配置档。
禁用严格路由可能导致 DNS 泄露。
错误: %1</translation>
    </message>
    <message>
        <source>Tun device misbehaving</source>
        <translation>Tun 设备运行异常</translation>
    </message>
    <message>
        <source>If you have trouble starting VPN, you can force reset Core process here and then try starting the profile again. The error is %1</source>
        <translation>如果启动 VPN 时有问题，您可以在这里强制重置 Core 进程，然后再次尝试启动配置档。错误是 %1</translation>
    </message>
    <message>
        <source>Reset</source>
        <translation>重置</translation>
    </message>
    <message>
        <source>Cancel</source>
        <translation>取消</translation>
    </message>
    <message>
        <source>Leftover Tun adapter</source>
        <translation>剩余 Tun 适配器</translation>
    </message>
    <message>
        <source>Windows was still holding the Tun adapter from an earlier session that did not close cleanly. Starting the profile again usually works; if it keeps failing, restart Windows.
Error: %1</source>
        <translation>Windows 仍在占用之前未正常关闭的会话中的 Tun 适配器。通常再次启动配置档就起效；如果一直失败，则重启 Windows。
错误: %1</translation>
    </message>
    <message>
        <source>Start Again</source>
        <translation>再次启动</translation>
    </message>
    <message>
        <source>[Auto selector] Running the best %1 of %2 ranked profiles.</source>
        <translation>[自动选择器] 正在运行排名最优的 %1 / %2 个配置档。</translation>
    </message>
    <message>
        <source>Another profile is starting...</source>
        <translation>另一个配置档正在启动...</translation>
    </message>
    <message>
        <source>Another profile is stopping...</source>
        <translation>另一个配置档正在停止...</translation>
    </message>
    <message>
        <source>Try to start the config, but the core has not listened to the RPC port, so restart it...</source>
        <translation>尝试启动该配置，但核心尚未监听到 RPC 端口，因此请重启它...</translation>
    </message>
    <message>
        <source>If there is no response for a long time, it is recommended to restart the software.</source>
        <translation>如果长时间没有反应，建议重启软件。</translation>
    </message>
    <message>
        <source>[Kill switch] Not starting %1: the kill switch is not active.</source>
        <translation>[急停开关] 未启动 %1: 急停开关未激活。</translation>
    </message>
    <message>
        <source>[Kill switch] The extra core %1 could not be allowed through the kill switch.</source>
        <translation>[急停开关] 附加核心 %1 无法允许通过该急停开关。</translation>
    </message>
    <message>
        <source>Starting profile %1</source>
        <translation>正在启动配置档 %1</translation>
    </message>
    <message>
        <source>Failed to start profile %1</source>
        <translation>启动配置档 %1 失败</translation>
    </message>
    <message>
        <source>Stop return error</source>
        <translation>停止返回错误</translation>
    </message>
    <message>
        <source>Stopping profile %1</source>
        <translation>正在停止配置档 %1</translation>
    </message>
    <message>
        <source>Failed to stop, please restart the program.</source>
        <translation>停止失败，请重启程序。</translation>
    </message>
    <message>
        <source>[VPN] %1: %2</source>
        <translation></translation>
    </message>
    <message>
        <source>VPN endpoint problem</source>
        <translation>VPN 端点问题</translation>
    </message>
    <message>
        <source>VPN endpoint</source>
        <translation>VPN 端点</translation>
    </message>
    <message>
        <source>[VPN] %1: the core did not answer the sign-in prompt.</source>
        <translation>[VPN] %1: 核心未应答登录提示。</translation>
    </message>
    <message>
        <source>[VPN] %1: could not answer the sign-in prompt: %2</source>
        <translation>[VPN] %1: 无法应答登录提示: %2</translation>
    </message>
    <message>
        <source>[VPN] %1: signed in again with a new one-time code.</source>
        <translation>[VPN] %1: 用新的一次性代码再次登录。</translation>
    </message>
    <message>
        <source>[VPN] %1 rejected the saved credentials; restarting the profile with a new one-time code.</source>
        <translation>[VPN] %1 拒绝了保存的凭据; 正在用新的一次性代码重启该配置档。</translation>
    </message>
    <message>
        <source>The server refused the credentials saved with this profile. Enter the ones to use for this session; the profile itself is left unchanged.</source>
        <translation>服务器拒绝了随这个配置档保存的凭据。请输入用于本次会话的凭据；该配置档本身将保持不变。</translation>
    </message>
    <message>
        <source>[VPN] %1 rejected the saved credentials.</source>
        <translation>[VPN] %1 拒绝了保存的凭据。</translation>
    </message>
    <message>
        <source>Clone %1 item(s)</source>
        <translation>克隆 %1 项</translation>
    </message>
    <message>
        <source>Remove %1 item(s) ?</source>
        <translation>删除 %1 项吗？</translation>
    </message>
    <message>
        <source>Copied %1 item(s)</source>
        <translation>复制了 %1 项</translation>
    </message>
    <message>
        <source>Copy</source>
        <translation>复制</translation>
    </message>
    <message>
        <source>Copied</source>
        <translation>已复制</translation>
    </message>
    <message>
        <source>QR Code not found</source>
        <translation>未找到二维码</translation>
    </message>
    <message>
        <source>Unable to capture screen</source>
        <translation>无法捕获屏幕</translation>
    </message>
    <message>
        <source>Remove %1 Invalid item(s) ?</source>
        <translation>移除 %1 个无效项吗？</translation>
    </message>
    <message>
        <source>No insecure configs found.</source>
        <translation>找不到不安全配置。</translation>
    </message>
    <message>
        <source>Remove %1 insecure config(s)?</source>
        <translation>移除 %1 个不安全配置吗?</translation>
    </message>
    <message>
        <source>Failed to resolve domains: %1</source>
        <translation>解析域名失败: %1</translation>
    </message>
    <message>
        <source>the core did not answer</source>
        <translation>核心不应答</translation>
    </message>
    <message>
        <source>Failed to resolve %1: %2</source>
        <translation>未能解析 %1: %2</translation>
    </message>
    <message>
        <source>Replace domain server addresses with their resolved IPs?</source>
        <translation>替换域名服务器地址为其解析后的 IP 地址吗？</translation>
    </message>
    <message>
        <source>Remove %1 Unavailable item(s) ?</source>
        <translation>移除 %1 个不可用项吗？</translation>
    </message>
    <message numerus="yes">
        <source>%n remote rule-set(s) refreshed</source>
        <translation>
            <numerusform>刷新了 %n 个远程规则集</numerusform>
        </translation>
    </message>
    <message>
        <source>Stop Sorting</source>
        <translation>停止排序</translation>
    </message>
    <message>
        <source>Type</source>
        <translation>类型</translation>
    </message>
    <message>
        <source>Security</source>
        <translation>安全</translation>
    </message>
    <message>
        <source>Include:</source>
        <translation>包含:</translation>
    </message>
    <message>
        <source>Out IP</source>
        <translation>出口 IP</translation>
    </message>
    <message>
        <source>Speed</source>
        <translation>速度</translation>
    </message>
    <message>
        <source>Latency</source>
        <translation>延迟</translation>
    </message>
    <message>
        <source>IP Out</source>
        <translation>IP 出口</translation>
    </message>
    <message>
        <source>Connect</source>
        <translation>连接</translation>
    </message>
    <message>
        <source>Select Profile</source>
        <translation>选择配置档</translation>
    </message>
    <message>
        <source>Select Routing</source>
        <translation>选择路由</translation>
    </message>
    <message>
        <source>OTP Codes</source>
        <translation>OTP 代码</translation>
    </message>
    <message>
        <source>Could not update the autostart entry:</source>
        <translation>无法更新自动启动条目:</translation>
    </message>
    <message>
        <source>Update all subscriptions?</source>
        <translation>更新所有订阅吗？</translation>
    </message>
    <message>
        <source>Enable AdBlock</source>
        <translation>启用 AdBlock (广告屏蔽)</translation>
    </message>
    <message>
        <source>Enable Warp</source>
        <translation>启用 Warp</translation>
    </message>
    <message>
        <source> Test result(s) copied to clipboard!</source>
        <translation> 测试结果已被复制到剪贴板!</translation>
    </message>
    <message>
        <source>All files (*)</source>
        <translation>所有文件 (*)</translation>
    </message>
    <message>
        <source>Config files (*.json *.conf *.txt *.yaml *.yml *.ini *.ovpn *.xml)</source>
        <translation>配置文件 (*.json *.conf *.txt *.yaml *.yml *.ini *.ovpn *.xml)</translation>
    </message>
    <message>
        <source>QR code images (*.png *.jpg *.jpeg *.bmp *.gif *.webp)</source>
        <translation>QR 码图像 (*.png *.jpg *.jpeg *.bmp *.gif *.webp)</translation>
    </message>
    <message>
        <source>Select profile files</source>
        <translation>选择配置档文件</translation>
    </message>
    <message>
        <source>Open Traffic Statistics…</source>
        <translation>打开流量统计…</translation>
    </message>
    <message>
        <source>routing profiles</source>
        <translation>路由配置档</translation>
    </message>
    <message>
        <source>User opted for no privilege req, some features may not work</source>
        <translation>用户选择无权限请求，某些功能可能不起作用</translation>
    </message>
    <message>
        <source>System proxy: %1</source>
        <translation>系统代理: %1</translation>
    </message>
    <message>
        <source>The group &quot;%1&quot; uses its own routing profile, so the running profile keeps it.</source>
        <translation>分组 &quot;%1&quot; 使用自己的路由配置档，因此运行中的配置档会继续保持使用它。</translation>
    </message>
    <message>
        <source>[Kill switch] Restarting the core so that its own traffic passes the kill switch...</source>
        <translation>[急停开关] 正在重启核心，以便其自身的流量通过该急停开关...</translation>
    </message>
    <message>
        <source>This installation cannot grant the core privileges by itself.</source>
        <translation>这个安装不能自行赋予核心特权。</translation>
    </message>
    <message>
        <source>Please give the core root privileges</source>
        <translation>请赋予核心 root 权限</translation>
    </message>
    <message>
        <source>Please run Throne as admin</source>
        <translation>请以管理员身份运行 Throne</translation>
    </message>
    <message>
        <source>Requesting permission</source>
        <translation>正在请求权限</translation>
    </message>
    <message>
        <source>Please Enter your password in the opened terminal, then try again</source>
        <translation>请在打开的终端中输入您的密码，然后重试</translation>
    </message>
    <message>
        <source>Restart as administrator was cancelled, Throne keeps running</source>
        <translation>取消了以管理员身份重启，Throne 仍在运行中</translation>
    </message>
    <message>
        <source>Disable kill switch</source>
        <translation>禁用急停开关</translation>
    </message>
    <message>
        <source>Turn off the kill switch? Traffic that does not go through Throne will no longer be blocked.</source>
        <translation>关闭该急停开关吗？不经过 Throne 的流量将不再被阻止。</translation>
    </message>
    <message>
        <source>The sing-box API is disabled. Set a listen port in Preferences &gt; Integration Settings &gt; Core APIs.</source>
        <translation>sing-box API 被禁用。在“首选项 &gt; 集成设置 &gt; 核心 API” 中设置一个监听端口。</translation>
    </message>
    <message>
        <source>Start a profile first; the dashboard is served by the running core.</source>
        <translation>首先启动一个配置档； 该仪表盘由正在运行的核心提供服务。</translation>
    </message>
    <message>
        <source>Web dashboard</source>
        <translation>网络仪表盘</translation>
    </message>
    <message>
        <source>The dashboard is not installed yet. Download it now?</source>
        <translation>仪表盘未安装。立即下载吗?</translation>
    </message>
    <message>
        <source>Cannot start</source>
        <translation>无法启动</translation>
    </message>
    <message>
        <source>A dashboard download is already running</source>
        <translation>仪表盘下载已在运行中</translation>
    </message>
    <message>
        <source>The core did not answer.</source>
        <translation>核心不应答。</translation>
    </message>
    <message>
        <source>Failed to install the dashboard</source>
        <translation>安装仪表盘失败</translation>
    </message>
    <message>
        <source>Update is unavailable while a background Throne (PID %1) runs from this installation; stop it first.</source>
        <translation>从这个安装中运行的后台 Throne (PID %1) 会导致更新不可用；请先停止它。</translation>
    </message>
    <message>
        <source>Last download request has not finished yet</source>
        <translation>上次的下载请求尚未完成</translation>
    </message>
    <message>
        <source>Failed to download update assets</source>
        <translation>下载更新资源失败</translation>
    </message>
    <message>
        <source>Group</source>
        <translation>分组</translation>
    </message>
    <message>
        <source>Not Running</source>
        <translation>未在运行</translation>
    </message>
    <message>
        <source>Listening on all interfaces (%1)</source>
        <translation>正在监听所有接口 (%1)</translation>
    </message>
    <message>
        <source>Select</source>
        <translation>选择</translation>
    </message>
    <message>
        <source>Select mode, double-click or press Enter to select a profile, press ESC to exit.</source>
        <translation>选择模式，双击或按 ENTER 键选择一个配置档，按 ESC 键退出。</translation>
    </message>
    <message>
        <source>Kill switch: not active</source>
        <translation>急停开关: 未激活</translation>
    </message>
    <message>
        <source>Disconnecting</source>
        <translation>正在断开连接</translation>
    </message>
    <message>
        <source>Disconnect</source>
        <translation>断开连接</translation>
    </message>
    <message>
        <source>route</source>
        <translation></translation>
    </message>
    <message>
        <source>aux</source>
        <translation></translation>
    </message>
    <message>
        <source>Traffic statistics are disabled</source>
        <translation>流量统计被禁用</translation>
    </message>
    <message>
        <source>Untick “Disable Traffic Stats” in Basic Settings → Core → Statistics to record speeds again.</source>
        <translation>在 “基本设置 → 核心 → 统计” 中取消勾选 “禁用流量统计”，就可以重新记录速度。</translation>
    </message>
    <message>
        <source>Connect OK</source>
        <translation>连接通过</translation>
    </message>
    <message>
        <source>Connecting</source>
        <translation>正在连接</translation>
    </message>
    <message>
        <source>Waiting for authentication</source>
        <translation>正在等待认证</translation>
    </message>
    <message>
        <source>Tunnel error</source>
        <translation>隧道错误</translation>
    </message>
    <message>
        <source>Testing</source>
        <translation>测试</translation>
    </message>
    <message>
        <source>Test Result</source>
        <translation>测试结果</translation>
    </message>
    <message>
        <source>Unavailable</source>
        <translation>不可用</translation>
    </message>
    <message>
        <source>Failed to stop tests</source>
        <translation>停止测试失败</translation>
    </message>
    <message>
        <source>a tested profile</source>
        <translation>已测试的配置档</translation>
    </message>
    <message>
        <source>[%1] test error: %2</source>
        <translation>[%1] 测试错误: %2</translation>
    </message>
    <message>
        <source>[%1] IP test error: %2</source>
        <translation>[%1] IP 测试错误: %2</translation>
    </message>
    <message>
        <source>Profile test aborted</source>
        <translation>配置档测试中止</translation>
    </message>
    <message>
        <source>Something is very wrong, the subject ent cannot be found!</source>
        <translation>发生了非常严重的问题，找不到主题 ent！</translation>
    </message>
    <message>
        <source>Profile manager data is corrupted, try again.</source>
        <translation>配置档管理器数据损坏，请重试。</translation>
    </message>
    <message>
        <source>The last url test did not exit completely, please wait. If it persists, please restart the program.</source>
        <translation>上次的 URL 测试未完全退出，请稍候。如果持续存在，则请重启程序。</translation>
    </message>
    <message>
        <source>The last test did not exit completely, please wait. If it persists, please restart the program.</source>
        <translation>上次的测试未完全退出，请稍候。如果情况持续，请重启该程序。</translation>
    </message>
    <message>
        <source>Failed to build test config for batch: </source>
        <translation>为批处理构建测试配置失败: </translation>
    </message>
    <message>
        <source>URL test finished!</source>
        <translation>URL 测试完成！</translation>
    </message>
    <message>
        <source>IP test finished!</source>
        <translation>IP 测试完成!</translation>
    </message>
    <message>
        <source>The last test did not finish completely, please wait. If it persists, please restart the program.</source>
        <translation>上次的测试未完全完成，请稍候。如果情况持续，请重启该程序。</translation>
    </message>
    <message>
        <source>Failed to build batch test config: </source>
        <translation>构建批处理测试配置失败: </translation>
    </message>
    <message>
        <source>Speedtest finished!</source>
        <translation>测速完成！</translation>
    </message>
    <message>
        <source>Profile speed test aborted</source>
        <translation>配置档速度测试已中止</translation>
    </message>
    <message>
        <source>[%1] speed test error: %2</source>
        <translation>[%1] 速度测试错误：%2</translation>
    </message>
    <message>
        <source>Trigger main window</source>
        <translation>显示/隐藏主窗口</translation>
    </message>
    <message>
        <source>Show groups</source>
        <translation>显示分组</translation>
    </message>
    <message>
        <source>Show routes</source>
        <translation>显示路由</translation>
    </message>
    <message>
        <source>Proxy mode</source>
        <translation>代理模式</translation>
    </message>
    <message>
        <source>Toggle System Proxy</source>
        <translation>切换系统代理</translation>
    </message>
    <message>
        <source>Start/Stop Profile</source>
        <translation>启动/停止配置档</translation>
    </message>
    <message>
        <source>Toggle Tun Mode</source>
        <translation>切换 Tun 模式</translation>
    </message>
    <message>
        <source>Global hotkey not registered: %1</source>
        <translation>全局热键未注册: %1</translation>
    </message>
    <message>
        <source>Search Settings</source>
        <translation>搜索设置</translation>
    </message>
    <message>
        <source>close the open settings window first</source>
        <translation>先关闭打开的设置窗口</translation>
    </message>
    <message>
        <source>close &quot;%1&quot; first</source>
        <translation>先关闭 &quot;%1&quot;</translation>
    </message>
    <message>
        <source>[Auto selector] Every running profile stopped working through the hops in front of it — rebuilding with the next candidates.</source>
        <translation>[自动选择器] 所有正在运行的配置档经由其前方的跳点均已失效 — 正在用下一批候选重新构建。</translation>
    </message>
    <message>
        <source>The core did not answer the test</source>
        <translation>核心未响应测试</translation>
    </message>
    <message>
        <source>Tun Mode and the kill switch need the core to run with administrator rights.

Grant them once with a Windows confirmation; Throne itself keeps running as a standard user.</source>
        <translation>Tun 模式和急停开关需要核心以管理员权限运行。

只需在 Windows 确认窗口中授予一次；Throne 本身仍以标准用户身份运行。</translation>
    </message>
    <message>
        <source>The update was cancelled, Throne keeps running</source>
        <translation>已取消更新，Throne 仍在运行中</translation>
    </message>
    <message>
        <source>Service Check</source>
        <translation>服务检查</translation>
    </message>
    <message>
        <source>Service Check Group</source>
        <translation>服务检查本组</translation>
    </message>
    <message>
        <source>Services</source>
        <translation>服务</translation>
    </message>
    <message>
        <source>A service check can test up to 100 profiles at a time. Select fewer profiles.</source>
        <translation>服务检查一次最多可检查 100 个配置档。请选择更少的配置档。</translation>
    </message>
    <message>
        <source>This checks %1 profiles. Each one sends about 20 requests to the checked services. Continue?</source>
        <translation>将检查 %1 个配置档。每个配置档会向被检查的服务发送约 20 个请求。是否继续？</translation>
    </message>
    <message>
        <source>Service check finished!</source>
        <translation>服务检查完成！</translation>
    </message>
    <message>
        <source>[%1] service check error: %2</source>
        <translation>[%1] 服务检查错误: %2</translation>
    </message>
    <message>
        <source>Service check failed: %1</source>
        <translation>服务检查失败: %1</translation>
    </message>
    <message>
        <source>Find working settings…</source>
        <translation>查找可用设置…</translation>
    </message>
    <message>
        <source>The core did not get administrator rights, although they were granted. The log shows why.

Restart Throne as administrator to use Tun Mode and the kill switch.</source>
        <translation>虽然已授予管理员权限，但核心未能以管理员身份运行。原因请查看日志。

请以管理员身份重启 Throne 以使用 Tun 模式和急停开关。</translation>
    </message>
    <message>
        <source>Throne can restart as administrator instead.</source>
        <translation>也可以改为以管理员身份重启 Throne。</translation>
    </message>
    <message>
        <source>Pin</source>
        <translation>置顶</translation>
    </message>
    <message>
        <source>Unpin</source>
        <translation>取消置顶</translation>
    </message>
    <message>
        <source>Deep Link</source>
        <translation>深度链接</translation>
    </message>
</context>
<context>
    <name>MuxSettingsWidget</name>
    <message>
        <source>Protocol</source>
        <translation>协议</translation>
    </message>
    <message>
        <source>Max Streams</source>
        <translation>最大流数</translation>
    </message>
    <message>
        <source>Max Connections</source>
        <translation>最大连接数</translation>
    </message>
    <message>
        <source>Mode</source>
        <translation>模式</translation>
    </message>
    <message>
        <source>&lt;html&gt;&lt;head/&gt;&lt;body&gt;&lt;p&gt;&lt;b&gt;Max Streams&lt;/b&gt;: reuse a connection until it carries this many streams, then open another.&lt;/p&gt;&lt;p&gt;&lt;b&gt;Max Connections&lt;/b&gt;: keep at most this many connections, and open a new one only while the least busy connection already carries Min Streams streams.&lt;/p&gt;&lt;/body&gt;&lt;/html&gt;</source>
        <translation>&lt;html&gt;&lt;head/&gt;&lt;body&gt;&lt;p&gt;&lt;b&gt;最大流数&lt;/b&gt;：重复使用一个连接，直到它承载这么多的流，然后再打开另一个。&lt;/p&gt;&lt;p&gt;&lt;b&gt;最大连接数&lt;/b&gt;：最多保持这么多的连接，只有当最不繁忙的连接已承载最少流数时，才会打开新的连接。&lt;/p&gt;&lt;/body&gt;&lt;/html&gt;</translation>
    </message>
    <message>
        <source>Min Streams</source>
        <translation>最小流数</translation>
    </message>
    <message>
        <source>Padding</source>
        <translation>填充</translation>
    </message>
    <message>
        <source>TCP Brutal</source>
        <translation>TCP Brutal</translation>
    </message>
    <message>
        <source>&lt;html&gt;&lt;head/&gt;&lt;body&gt;&lt;p&gt;TCP Brutal congestion control at the speeds below; the server must support it. Every stream then shares one connection, so the mode is ignored.&lt;/p&gt;&lt;/body&gt;&lt;/html&gt;</source>
        <translation>&lt;html&gt;&lt;head/&gt;&lt;body&gt;&lt;p&gt;在以下速度下使用 TCP Brutal 拥塞控制；服务器必须支持它。然后每个流都会共享一个连接，所以该模式会被忽略。&lt;/p&gt;&lt;/body&gt;&lt;/html&gt;</translation>
    </message>
    <message>
        <source>Upload (Mb/s)</source>
        <translation>上传 (Mb/秒)</translation>
    </message>
    <message>
        <source>Download (Mb/s)</source>
        <translation>下载 (Mb/秒)</translation>
    </message>
    <message>
        <source>Custom Multiplex</source>
        <translation>自定义复用</translation>
    </message>
</context>
<context>
    <name>OtpItem</name>
    <message>
        <source>Edit</source>
        <translation>编辑</translation>
    </message>
    <message>
        <source>Export</source>
        <translation>导出</translation>
    </message>
    <message>
        <source>Delete</source>
        <translation>删除</translation>
    </message>
    <message>
        <source>invalid</source>
        <translation>无效</translation>
    </message>
    <message>
        <source>%1s</source>
        <translation>%1秒</translation>
    </message>
</context>
<context>
    <name>ProfilesTableFilterHeader</name>
    <message>
        <source>Filter...</source>
        <translation>筛选...</translation>
    </message>
    <message>
        <source>Filter by country or service...</source>
        <translation>按国家或服务筛选...</translation>
    </message>
    <message>
        <source>Countries and working services, e.g. &quot;DE gemini&quot; or &quot;ai&quot;.
&quot;!netflix&quot; finds profiles Netflix refused.</source>
        <translation>国家和可用的服务，例如“DE gemini”或“ai”。
“!netflix”查找 Netflix 拒绝访问的配置。</translation>
    </message>
    <message>
        <source>Disable Filter</source>
        <translation>禁用筛选</translation>
    </message>
    <message>
        <source>Enable Filter</source>
        <translation>启用筛选</translation>
    </message>
</context>
<context>
    <name>ProfilesTableModel</name>
    <message>
        <source>Certificate checks are turned off by the &quot;Skip TLS certificate authentication&quot; setting.</source>
        <translation>证书检查被“跳过 TLS 证书验证”设置关闭。</translation>
    </message>
    <message>
        <source>This config&apos;s traffic is not properly protected.</source>
        <translation>这个配置的流量未适当保护。</translation>
    </message>
    <message>
        <source>Type</source>
        <translation>类型</translation>
    </message>
    <message>
        <source>Address</source>
        <translation>地址</translation>
    </message>
    <message>
        <source>Name</source>
        <translation>名称</translation>
    </message>
    <message>
        <source>Test Result</source>
        <translation>测试结果</translation>
    </message>
    <message>
        <source>Traffic</source>
        <translation>流量</translation>
    </message>
</context>
<context>
    <name>ProxyItem</name>
    <message>
        <source>Confirmation</source>
        <translation>确认</translation>
    </message>
    <message>
        <source>Remove %1?</source>
        <translation>删除 %1 吗?</translation>
    </message>
</context>
<context>
    <name>QCoreApplication::QObject</name>
    <message>
        <source>Not an otpauth link or secret: %1</source>
        <translation>不是 otpauth 链接或密钥: %1</translation>
    </message>
</context>
<context>
    <name>QGuiApplication</name>
    <message>
        <source>QT_LAYOUT_DIRECTION</source>
        <translation>LTR</translation>
    </message>
</context>
<context>
    <name>QObject</name>
    <message>
        <source>Auto Selector</source>
        <translation>自动选择器</translation>
    </message>
    <message>
        <source>Chain Proxy</source>
        <translation>链式代理</translation>
    </message>
    <message>
        <source>Direct</source>
        <translation>直连</translation>
    </message>
    <message>
        <source>Endpoint</source>
        <translation>端点</translation>
    </message>
    <message>
        <source>missing profile</source>
        <translation>缺少配置档</translation>
    </message>
    <message>
        <source>chain or auto selector</source>
        <translation>链式或自动选择器</translation>
    </message>
    <message>
        <source>needs too many core switches</source>
        <translation>需要过多的核心切换</translation>
    </message>
    <message>
        <source>extra-core profile</source>
        <translation>附加核心配置档</translation>
    </message>
    <message>
        <source>full config profile</source>
        <translation>完整配置档</translation>
    </message>
    <message>
        <source>config does not parse</source>
        <translation>配置解析失败</translation>
    </message>
    <message>
        <source>Tailscale profile</source>
        <translation>Tailscale 配置档</translation>
    </message>
    <message>
        <source>OpenVPN or OpenConnect profile</source>
        <translation>OpenVPN 或 OpenConnect 配置档</translation>
    </message>
    <message>
        <source>filtered out by name</source>
        <translation>按名称过滤掉</translation>
    </message>
    <message>
        <source>filtered out by country</source>
        <translation>按国别过滤掉</translation>
    </message>
    <message>
        <source>last test failed</source>
        <translation>个上次测试失败项</translation>
    </message>
    <message>
        <source>Xray full config cannot be combined with the group&apos;s proxies</source>
        <translation>Xray 完整配置无法与该组的代理一起使用</translation>
    </message>
    <message>
        <source>Reality</source>
        <translation>Reality</translation>
    </message>
    <message>
        <source>Insecure TLS</source>
        <translation>不安全 TLS</translation>
    </message>
    <message>
        <source>Private</source>
        <translation>私有</translation>
    </message>
    <message>
        <source>Raw download</source>
        <translation>原始下载</translation>
    </message>
    <message>
        <source>Insecure TLS download</source>
        <translation>不安全的 TLS 下载</translation>
    </message>
    <message>
        <source>Compromised</source>
        <translation>折衷</translation>
    </message>
    <message>
        <source>TLS</source>
        <translation>TLS</translation>
    </message>
    <message>
        <source>Raw</source>
        <translation>原始</translation>
    </message>
    <message>
        <source>%1 dials its own address: %2</source>
        <translation>%1 拨打它自己的地址: %2</translation>
    </message>
    <message>
        <source>The routing profile lists an endpoint profile (id %1) that no longer exists</source>
        <translation>路由配置中引用了一个不再存在的端点配置 (id %1)</translation>
    </message>
    <message>
        <source>%1 is listed as a routing profile endpoint but is empty or corrupted</source>
        <translation>%1 被列为路由配置档端点，但内容为空或已损坏</translation>
    </message>
    <message>
        <source>%1 is listed as a routing profile endpoint but a hop of it no longer exists</source>
        <translation>%1 被列为路由配置档端点，但它的其中一跳转节点不再存在</translation>
    </message>
    <message>
        <source>%1 is listed as a routing profile endpoint, so its last hop must be an OpenVPN or OpenConnect profile</source>
        <translation>%1 被列为路由配置档端点，因此它的最后一跳节点必须是 OpenVPN 或 OpenConnect 配置档</translation>
    </message>
    <message>
        <source>Hops of the routing profile endpoint %1 cannot use an extra core, a full config, or be a chain</source>
        <translation>路由配置档端点 %1 的跳转节点不能使用额外核心、完整配置，也不能构成链</translation>
    </message>
    <message>
        <source>Hops of the routing profile endpoint %1 cannot run on the Xray core</source>
        <translation>路由配置档端点 %1 的跳转节点不能运行在 Xray 核心上</translation>
    </message>
    <message>
        <source>%1 is used as an endpoint of the routing profile and by the started profile at the same time, remove it from one of them</source>
        <translation>%1 同时被用作路由配置档的端点和被启动的配置档使用，请从其中一个中移除它</translation>
    </message>
    <message>
        <source>%1 is listed twice in the endpoints of the routing profile</source>
        <translation>%1 在路由配置档的端点中重复出现</translation>
    </message>
    <message>
        <source>Skipped the Tun private range &quot;%1&quot;: it is not a valid IP address or CIDR.</source>
        <translation>跳过了 Tun 私有范围 “%1”：它不是有效的 IP 地址或 CIDR。</translation>
    </message>
    <message>
        <source>DNS address &quot;%1&quot; is invalid: %2</source>
        <translation>DNS 地址 “%1” 无效: %2</translation>
    </message>
    <message>
        <source>Chain &quot;%1&quot; has a hop (profile id %2) that no longer exists, edit the chain to remove it</source>
        <translation>链“%1”中有一个不再存在的跳点（配置档ID %2），请编辑链以将其移除</translation>
    </message>
    <message>
        <source>Group &quot;%1&quot; has a front proxy that has been deleted.</source>
        <translation>分组“%1”的前置代理已被删除。</translation>
    </message>
    <message>
        <source>Group &quot;%1&quot; has a landing proxy that has been deleted.</source>
        <translation>组“%1”有一个已被删除的落地代理。</translation>
    </message>
    <message>
        <source>A proxy in the chain can no longer be found.</source>
        <translation>链中的某个代理找不到了。</translation>
    </message>
    <message>
        <source>The profile no longer exists</source>
        <translation>该配置档不再存在</translation>
    </message>
    <message>
        <source>%1 profiles have no single server to scan</source>
        <translation>%1 个配置档没有要扫描的单个服务器</translation>
    </message>
    <message>
        <source>Hysteria2 realm profiles have no fixed server to scan</source>
        <translation>Hysteria2 realm (领域)配置的那个没有要扫描的固定服务器</translation>
    </message>
    <message>
        <source>The chain has no hops</source>
        <translation>该（代理）链没有跳（点）</translation>
    </message>
    <message>
        <source>A hop of the chain no longer exists</source>
        <translation>该链条的跳点不再存在</translation>
    </message>
    <message>
        <source>Chains with an extra-core or Xray full config hop cannot be tested</source>
        <translation>带有附加核心或 Xray 完全配置的跳点的（代理）链无法测试</translation>
    </message>
    <message>
        <source>Skipped testing %1: %2</source>
        <translation>跳过了正在测试的 %1: %2</translation>
    </message>
    <message>
        <source>the group&apos;s fixed address</source>
        <translation>该分组的固定地址</translation>
    </message>
    <message>
        <source>The IP list no longer exists</source>
        <translation>该 IP 列表不再存在</translation>
    </message>
    <message>
        <source>IP list &quot;%1&quot;</source>
        <translation>IP 列表 &quot;%1&quot;</translation>
    </message>
    <message>
        <source>The IP list &quot;%1&quot; is empty</source>
        <translation>IP 列表 &quot;%1&quot; 是空的</translation>
    </message>
    <message>
        <source>%1 profiles have no single server address to replace</source>
        <translation>%1 个配置档没有要替换的单一服务器地址</translation>
    </message>
    <message>
        <source>Hysteria2 realm profiles have no fixed server address to replace</source>
        <translation>Hysteria2 realm 配置档没有要替换的固定服务器地址</translation>
    </message>
    <message>
        <source>The profile could not be copied for the scan</source>
        <translation>无法复制该配置档用于该扫描</translation>
    </message>
    <message>
        <source>The profile could not be checked: core unreachable</source>
        <translation>该配置档无法被检查: 核心不可达</translation>
    </message>
    <message>
        <source>The profile is not valid; the log has the details</source>
        <translation>配置档无效; 日志有详细信息</translation>
    </message>
    <message>
        <source>The profile cannot be pointed at the scanned addresses</source>
        <translation>该配置档无法指向已扫描的地址</translation>
    </message>
    <message>
        <source>no group</source>
        <translation>无分组</translation>
    </message>
    <message>
        <source>missing group</source>
        <translation>缺少分组</translation>
    </message>
    <message>
        <source>Encrypted</source>
        <translation>加密</translation>
    </message>
    <message>
        <source>Insecure</source>
        <translation>不安全</translation>
    </message>
    <message>
        <source>Static Key</source>
        <translation>静态钥匙</translation>
    </message>
    <message>
        <source>Unverified TLS</source>
        <translation>未验证的 TLS</translation>
    </message>
    <message>
        <source>Plaintext Replies</source>
        <translation>纯文本响应</translation>
    </message>
    <message>
        <source>Weak Cipher</source>
        <translation>弱密码算法</translation>
    </message>
    <message>
        <source>Removed %1 duplicate profiles:</source>
        <translation>移除了 %1 个重复配置档:</translation>
    </message>
    <message>
        <source>Removed %1 insecure profiles:</source>
        <translation>移除了 %1 个不安全配置档:</translation>
    </message>
    <message>
        <source>Skipped removing invalid profiles: the core is unreachable.</source>
        <translation>跳过正在移除的无效配置档操作: 无法访问核心。</translation>
    </message>
    <message>
        <source>Removed %1 invalid profiles:</source>
        <translation>移除了 %1 个无效配置档:</translation>
    </message>
    <message>
        <source>The running profile was kept.</source>
        <translation>正在运行的配置档被保留。</translation>
    </message>
    <message>
        <source>Removed %1 unavailable profiles:</source>
        <translation>移除了 %1 个不可用配置档:</translation>
    </message>
    <message>
        <source>%1 was removed by the update and stays stopped: no single profile with the same name replaces it.</source>
        <translation>%1 已被该更新移除并保持停止状态：没有同名的单个配置档替换它。</translation>
    </message>
    <message>
        <source>%1 was replaced by the update; connecting to the profile with the same name.</source>
        <translation>%1 已被该更新替换；正在连接到同名的配置档。</translation>
    </message>
    <message>
        <source>%1 was changed by the update while running; the change applies on its next start.</source>
        <translation>%1 在运行期间已被该更新修改；更改将在下次启动时生效。</translation>
    </message>
    <message>
        <source>%1 was changed by the update; reconnecting with its new settings.</source>
        <translation>%1 已被该更新修改；正在以新设置重新连接。</translation>
    </message>
    <message>
        <source>Subscription update already queued: %1</source>
        <translation>订阅更新已排队: %1</translation>
    </message>
    <message>
        <source>Requesting subscription: %1</source>
        <translation>正在请求订阅: %1</translation>
    </message>
    <message>
        <source>Requesting subscription %1 error: %2</source>
        <translation>请求订阅 %1 时出错: %2</translation>
    </message>
    <message>
        <source>Subscription request fininshed: %1</source>
        <translation>完成了订阅请求: %1</translation>
    </message>
    <message>
        <source>Subscription server update interval: %1 hour(s)</source>
        <translation>订阅服务器更新间隔: %1 小时</translation>
    </message>
    <message>
        <source>Processing subscription data...</source>
        <translation>正在处理订阅数据...</translation>
    </message>
    <message>
        <source>Process complete, applying...</source>
        <translation>处理完成，正在应用...</translation>
    </message>
    <message>
        <source>The URL of %1 changed during the update, so the result was discarded.</source>
        <translation>在更新期间 %1 的 URL 被更改，因此该结果被丢弃。</translation>
    </message>
    <message>
        <source>The server returned a web page instead of a subscription. If it is a CAPTCHA or a bot check, try another TLS version, HTTP version or User Agent in the group&apos;s Advanced settings.</source>
        <translation>服务器返回了一个网页，而不是订阅。如果这是 CAPTCHA 或机器人检查，尝试在分组的高级设置中更换“TLS 版本”、“HTTP 版本”或“用户代理”。</translation>
    </message>
    <message>
        <source>Announcement from %1: %2</source>
        <translation>通告来自 %1: %2</translation>
    </message>
    <message>
        <source>No profiles found in the subscription: %1 was left unchanged.</source>
        <translation>该订阅中未找到任何配置档: %1 保持不变。</translation>
    </message>
    <message>
        <source>Clearing servers...</source>
        <translation>正在清空服务器...</translation>
    </message>
    <message>
        <source>Added %1 profiles:
%2
Updated %3 profiles:
%4
Deleted %5 Profiles:
%6</source>
        <translation>增加了 %1 个配置档：
%2
更新了 %3 个配置档：
%4
删除了 %5 个配置档：
%6</translation>
    </message>
    <message>
        <source>Still in use, so kept instead of deleted:
%1</source>
        <translation>仍在使用中，因此保留而不是删除:
%1</translation>
    </message>
    <message>
        <source>Working, so kept instead of deleted:
%1</source>
        <translation>工作正常，因此保留而非删除:
%1</translation>
    </message>
    <message>
        <source>Nothing</source>
        <translation>无</translation>
    </message>
    <message>
        <source>Change of %1:</source>
        <translation>「%1」的变化:</translation>
    </message>
    <message>
        <source>Change of %1</source>
        <translation>「%1」的变化</translation>
    </message>
    <message>
        <source>After the URL test of %1:</source>
        <translation>在 %1 进行 URL 测试后:</translation>
    </message>
    <message>
        <source>internal error: null profile</source>
        <translation>内部错误： 虚配置档</translation>
    </message>
    <message>
        <source>not a remote routing profile</source>
        <translation>不是一个远程路由配置档</translation>
    </message>
    <message>
        <source>remote URL is empty</source>
        <translation>远程 URL 为空</translation>
    </message>
    <message>
        <source>could not parse a routing profile from the response</source>
        <translation>无法解析响应中的路由配置档</translation>
    </message>
    <message>
        <source>the routing profile is not in the Happ format</source>
        <translation>该路由配置档不是 Happ 格式的</translation>
    </message>
    <message>
        <source>a raw routing profile is not accepted from a subscription</source>
        <translation>不接受来自订阅的原始路由配置档</translation>
    </message>
    <message>
        <source>unknown routing link action &quot;%1&quot;</source>
        <translation>未知的路由链接动作 &quot;%1&quot;</translation>
    </message>
    <message>
        <source>the routing profile is not valid base64</source>
        <translation>该路由配置档不是有效的 base64</translation>
    </message>
    <message>
        <source>the routing profile is not a JSON object</source>
        <translation>该路由配置档不是 JSON 对象</translation>
    </message>
    <message>
        <source>[Routing] %1: the routing profile the subscription provides was ignored: %2</source>
        <translation>[路由] %1: 已忽略该订阅提供的路由配置档: %2</translation>
    </message>
    <message>
        <source>[Routing] %1: the routing profile the subscription provides was not applied, the routing profiles are open for editing.</source>
        <translation>[路由] %1: 订阅提供的路由配置档未被应用；路由配置档仍可编辑。</translation>
    </message>
    <message>
        <source>%1 entries, %2 skipped</source>
        <translation>%1 项条目,跳过了 %2 项</translation>
    </message>
    <message>
        <source>%1 entries</source>
        <translation>%1 项条目</translation>
    </message>
    <message>
        <source>%1 routing</source>
        <translation>%1 路由</translation>
    </message>
    <message>
        <source>[Routing] %1: created the routing profile &quot;%2&quot; from the subscription (%3)</source>
        <translation>[路由] %1: 创建路由配置档 &quot;%2&quot; 自订阅 (%3)</translation>
    </message>
    <message>
        <source>[Routing] %1: updated the routing profile &quot;%2&quot; from the subscription (%3)</source>
        <translation>[路由] %1: 更新了路由配置档 &quot;%2&quot; 自订阅 (%3)</translation>
    </message>
    <message>
        <source>the remote content is a raw routing profile, which is not supported for remote profiles yet</source>
        <translation>远程内容是一个原始的路由配置档，它尚未被远程配置档支持</translation>
    </message>
    <message>
        <source>Updating remote routing profile: %1</source>
        <translation>正在更新远程路由器配置档: %1</translation>
    </message>
    <message>
        <source>Remote routing profile %1 failed: %2</source>
        <translation>远程路由配置档 %1 失败: %2</translation>
    </message>
    <message>
        <source>Remote routing profile updated: %1</source>
        <translation>远程路由配置档已更新: %1</translation>
    </message>
    <message>
        <source>Remote routing profiles: %1 updated, %2 failed</source>
        <translation>远程路由配置档： 更新了 %1 个，失败了 %2 个</translation>
    </message>
    <message>
        <source>Ignored %1 %2: %3</source>
        <translation>忽略了 %1 %2: %3</translation>
    </message>
    <message>
        <source>Ignored an unreadable value: %1 %2</source>
        <translation>忽略了不可读取的值: %1 %2</translation>
    </message>
    <message>
        <source>Empty OpenVPN configuration.</source>
        <translation>空 OpenVPN 配置。</translation>
    </message>
    <message>
        <source>&lt;connection&gt; block without a remote, skipped.</source>
        <translation>&lt;connection&gt; 块未带 remote，已跳过。</translation>
    </message>
    <message>
        <source>Ignored an unreadable peer fingerprint: %1</source>
        <translation>已忽略不可读取的对端指纹: %1</translation>
    </message>
    <message>
        <source>PKCS#12 bundles are not supported; export the CA, certificate and key as PEM.</source>
        <translation>不支持 PKCS#12 捆绑包；请将 CA 证书、客户端证书和钥匙导出为 PEM 格式。</translation>
    </message>
    <message>
        <source>Ignored inline block: &lt;%1&gt;</source>
        <translation>已忽略内联块： &lt;%1&gt;</translation>
    </message>
    <message>
        <source>This is an OpenVPN server configuration (%1), not a client profile.</source>
        <translation>这是 OpenVPN 服务器配置 (%1)，不是客户端配置档。</translation>
    </message>
    <message>
        <source>TAP (layer 2) tunnels are not supported; only `dev tun` profiles can be imported.</source>
        <translation>不支持 TAP (第2层) 隧道，仅可以导入 `dev tun` 配置档。</translation>
    </message>
    <message>
        <source>This is an OpenVPN server configuration (mode server), not a client profile.</source>
        <translation>这是 OpenVPN 服务器配置(模式服务器)，不是客户端配置档。</translation>
    </message>
    <message>
        <source>`proto %1` is a server transport.</source>
        <translation>`proto %1` 是服务器传输（协议）。</translation>
    </message>
    <message>
        <source>Unknown transport: proto %1</source>
        <translation>未知传输： 协议 %1</translation>
    </message>
    <message>
        <source>Ignored an unreadable IPv6 interface address: %1</source>
        <translation>已忽略无法读取的 IPv6 接口地址: %1</translation>
    </message>
    <message>
        <source>Credentials live in %1; enter them in the profile editor.</source>
        <translation>凭据存在 %1 中；请在配置档编辑器中输入。</translation>
    </message>
    <message>
        <source>A hash-directory CRL is not supported; point `crl-verify` at a PEM or DER file.</source>
        <translation>不支持哈希目录形式的 CRL；请在 PEM 或 DER 文件处指向 `crl-verify`。</translation>
    </message>
    <message>
        <source>Ignored a symbolic route target: route %1</source>
        <translation>已忽略符号路由目标: 路由 %1</translation>
    </message>
    <message>
        <source>Ignored an unreadable route: %1</source>
        <translation>已忽略不可达的路由: %1</translation>
    </message>
    <message>
        <source>redirect-gateway flag has no sing-box equivalent: %1</source>
        <translation>redirect-gateway (重定向网关)标志没有对应的 sing-box 设置： %1</translation>
    </message>
    <message>
        <source>Not supported by the OpenVPN endpoint, ignored: %1</source>
        <translation>不被 OpenVPN 端点支持，已忽略： %1</translation>
    </message>
    <message>
        <source>Unknown OpenVPN directive, ignored: %1</source>
        <translation>未知的 OpenVPN 指令，已忽略： %1</translation>
    </message>
    <message>
        <source>`%1` turns the client certificate off; it was dropped and the server has to accept password login.</source>
        <translation>`%1` 关闭客户端证书；证书已被取消，服务器必须接受密码登录。</translation>
    </message>
    <message>
        <source>`cert` without a `key`: add the private key, or `client-cert-not-required` for password-only login.</source>
        <translation>`cert` 没有 `key`: 添加私钥，或使用 `client-cert-not-required` 进行仅密码登录。</translation>
    </message>
    <message>
        <source>`key` without a `cert`: add the client certificate.</source>
        <translation>`key` 没有 `cert`: 添加客户端证书。</translation>
    </message>
    <message>
        <source>No `remote` server in the OpenVPN configuration.</source>
        <translation>OpenVPN 配置中没有 `remote` 服务器。</translation>
    </message>
    <message>
        <source>Ignored an unreadable interface address: %1</source>
        <translation>忽略了无法读取的接口地址： %1</translation>
    </message>
    <message>
        <source>`remote-cert-eku` replaces `remote-cert-tls`; the latter was dropped.</source>
        <translation>`remote-cert-eku` 替代了 `remote-cert-tls`；后者已被弃用。</translation>
    </message>
    <message>
        <source>Unsupported OpenConnect protocol, ignored: %1</source>
        <translation>不支持的 OpenConnect 协议，已忽略： %1</translation>
    </message>
    <message>
        <source>Unknown compression mode, ignored: %1</source>
        <translation>未知的压缩模式，已忽略： %1</translation>
    </message>
    <message>
        <source>Raised %1 %2 to 2s, the shortest interval the endpoint uses.</source>
        <translation>提高 %1 %2 到 2 秒，该端点使用的最短间隔。</translation>
    </message>
    <message>
        <source>Unknown reported OS, ignored: %1</source>
        <translation>未知的报告操作系统，已忽略： %1</translation>
    </message>
    <message>
        <source>Unsupported token mode, ignored: %1</source>
        <translation>不支持的令牌模式，已忽略： %1</translation>
    </message>
    <message>
        <source>Expected --form-entry=FORM:OPTION=VALUE, ignored: %1</source>
        <translation>预期 --form-entry=FORM:OPTION=VALUE，已忽略： %1</translation>
    </message>
    <message>
        <source>Configure a proxy through Throne&apos;s chain instead, ignored: %1</source>
        <translation>改为通过 Throne 的链来配置代理，已忽略： %1</translation>
    </message>
    <message>
        <source>Unknown OpenConnect option, ignored: %1</source>
        <translation>未知的 OpenConnect 选项，已忽略： %1</translation>
    </message>
    <message>
        <source>Unknown OpenConnect option, ignored: -%1</source>
        <translation>未知的 OpenConnect 选项，已忽略： -%1</translation>
    </message>
    <message>
        <source>An AnyConnect profile carrying a DTD is not accepted.</source>
        <translation>承载 DTD 的 AnyConnect 配置档不被接受。</translation>
    </message>
    <message>
        <source>Malformed AnyConnect profile: %1</source>
        <translation>格式错误的 AnyConnect 配置档： %1</translation>
    </message>
    <message>
        <source>The AnyConnect profile lists no host entry.</source>
        <translation>AnyConnect 配置档未列出主机条目。</translation>
    </message>
    <message>
        <source>Skipped a host entry without an address.</source>
        <translation>跳过了无地址的主机条目。</translation>
    </message>
    <message>
        <source>Skipped &quot;%1&quot;: IKEv2/IPsec is not spoken by the OpenConnect endpoint.</source>
        <translation>跳过了 &quot;%1&quot;： OpenConnect 端点不使用 IKEv2/IPsec。</translation>
    </message>
    <message>
        <source>Skipped an unreadable host address: %1</source>
        <translation>跳过了无法读取的主机地址: %1</translation>
    </message>
    <message>
        <source>Empty OpenConnect profile.</source>
        <translation>空的 OpenConnect 配置档。</translation>
    </message>
    <message>
        <source>The AnyConnect profile has no host entry this endpoint can use.</source>
        <translation>AnyConnect 配置档没有这个终端可以使用的主机条目。</translation>
    </message>
    <message>
        <source>No OpenConnect options found.</source>
        <translation>未找到 OpenConnect 选项。</translation>
    </message>
    <message>
        <source>No OpenConnect server address found.</source>
        <translation>未找到 OpenConnect 服务器地址。</translation>
    </message>
    <message>
        <source>Request with proxy but no profile started.</source>
        <translation>有用代理的请求，但未启动配置档。</translation>
    </message>
    <message>
        <source>Failed to reach the core.</source>
        <translation>连接到核心失败。</translation>
    </message>
    <message>
        <source>The core returned an invalid QUIC signature.</source>
        <translation>核心返回了无效的 QUIC 签名。</translation>
    </message>
    <message>
        <source>Failed to generate WARP QUIC signature: %1</source>
        <translation>生成 WARP QUIC 签名失败: %1</translation>
    </message>
    <message>
        <source>Cloudflare WARP</source>
        <translation>Cloudflare WARP</translation>
    </message>
    <message>
        <source>Generating a WARP identity registers a new device with Cloudflare.&lt;br&gt;&lt;br&gt;Do you accept the &lt;a href=&quot;%1&quot;&gt;Cloudflare WARP terms of service&lt;/a&gt;?</source>
        <translation>生成 WARP 身份识别会用 Cloudflare 注册新设备。&lt;br&gt;&lt;br&gt;你接受&lt;a href=&quot;%1&quot;&gt;Cloudflare WARP 服务条款&lt;/a&gt;吗?</translation>
    </message>
    <message>
        <source>Connect OK</source>
        <translation>连接成功</translation>
    </message>
    <message>
        <source>Unavailable</source>
        <translation>不可用</translation>
    </message>
    <message>
        <source>%1 route prefer</source>
        <translation>%1 路由优选</translation>
    </message>
    <message>
        <source>Default</source>
        <translation>默认</translation>
    </message>
    <message>
        <source>Request with proxy but the mixed inbound is disabled and Tun Mode is off.</source>
        <translation>用代理请求，但混合入站被禁用且 Tun 模式被关闭。</translation>
    </message>
    <message>
        <source>The selected TLS version is not supported on this system.</source>
        <translation>这个系统上不支持所选的 TLS 版本。</translation>
    </message>
    <message>
        <source>Response larger than %1 MB</source>
        <translation>响应大于 %1 MB</translation>
    </message>
    <message>
        <source>Download stalled: no data arrived for %1 seconds.</source>
        <translation>下载停滞: 持续 %1 秒没有数据到达。</translation>
    </message>
    <message>
        <source>Download failed: server returned HTTP status %1.</source>
        <translation>下载失败： 服务器返回 HTTP 状态 %1。</translation>
    </message>
    <message>
        <source>Download failed: the server returned an empty response.</source>
        <translation>下载失败： 服务器返回一个空响应。</translation>
    </message>
    <message>
        <source>Could not open file.</source>
        <translation>无法打开文件。</translation>
    </message>
    <message>
        <source>Could not write file.</source>
        <translation>无法写入文件。</translation>
    </message>
    <message>
        <source>Could not save downloaded file.</source>
        <translation>无法保存下载的文件。</translation>
    </message>
    <message>
        <source>Secret is empty</source>
        <translation>密钥是空的</translation>
    </message>
    <message>
        <source>Secret is not valid base32</source>
        <translation>密钥不是有效的 base32</translation>
    </message>
    <message>
        <source>Secret is too short</source>
        <translation>密钥太短</translation>
    </message>
    <message>
        <source>Digits must be between %1 and %2</source>
        <translation>数字必须在 %1 和 %2 之间</translation>
    </message>
    <message>
        <source>Period must be between %1 and %2 seconds</source>
        <translation>周期必须介于 %1 和 %2 秒之间</translation>
    </message>
    <message>
        <source>Not a readable OTP export</source>
        <translation>不是可读的 OTP 导出</translation>
    </message>
    <message>
        <source>Unreadable migration link</source>
        <translation>无法读取的迁移链接</translation>
    </message>
    <message>
        <source>Auto-update: running %1</source>
        <translation>自动更新： 正在运行 %1</translation>
    </message>
    <message>
        <source>Throne is running in the background (PID %1). Stop the service or use thronectl.</source>
        <translation>Throne 正在后台运行 (PID %1)。请停止该服务或使用 thronectl。</translation>
    </message>
    <message>
        <source>Throne is running in the background. Stop the service or use thronectl.</source>
        <translation>Throne 正在后台运行。请停止该服务或使用 thronectl。</translation>
    </message>
    <message>
        <source>Another Throne instance (PID %1)</source>
        <translation>另一个 Throne 实例 (PID %1)</translation>
    </message>
    <message>
        <source>Another Throne instance</source>
        <translation>另一个 Throne 实例</translation>
    </message>
    <message>
        <source>%1 is using this data directory:
%2</source>
        <translation>%1 正在使用这个数据目录:
%2</translation>
    </message>
    <message>
        <source>[Warn] Throne did not shut down cleanly last time. Diagnostics were saved to: %1</source>
        <translation>[警告] 上次 Throne 没有正常干净的关闭。诊断信息已被保存到： %1</translation>
    </message>
    <message>
        <source>never</source>
        <translation>从不</translation>
    </message>
    <message>
        <source>%1s ago</source>
        <translation>%1 秒前</translation>
    </message>
    <message>
        <source>%1m ago</source>
        <translation>%1 分钟前</translation>
    </message>
    <message>
        <source>%1h ago</source>
        <translation>%1 小时前</translation>
    </message>
    <message>
        <source>Auto selector paused — no network connection (%1 profiles held)</source>
        <translation>自动选择器已暂停 — 没有网络连接(保留了 %1 个配置档)</translation>
    </message>
    <message>
        <source>Auto selector starting — checking %1 profiles</source>
        <translation>自动选择器正在启动 — 正在检查 %1 个配置档</translation>
    </message>
    <message>
        <source>Auto selector checking profiles (%1/%2 measured)</source>
        <translation>自动选择器正在检查配置档 (已测量 %1/%2 个)</translation>
    </message>
    <message>
        <source>Auto selector — no working profile out of %1, still checking</source>
        <translation>自动选择器 — 在 %1 个配置档中无可用的配置档，仍在检查中</translation>
    </message>
    <message>
        <source>Auto selector on %1 (%2 of %3 working)</source>
        <translation>自动选择器在使用「%1」 (可用 %2 个/共 %3 个)</translation>
    </message>
    <message>
        <source>, switched %1</source>
        <translation>, 切换于 %1</translation>
    </message>
    <message>
        <source>%1 working</source>
        <translation>%1 个正在运作</translation>
    </message>
    <message>
        <source>%1 cooling down</source>
        <translation>%1 个正在冷却</translation>
    </message>
    <message>
        <source>%1 not checked yet</source>
        <translation>%1 个尚未检查</translation>
    </message>
    <message>
        <source>%1 being checked</source>
        <translation>%1 个正在检查</translation>
    </message>
    <message>
        <source>balancing over %1</source>
        <translation>在 %1 个上负载均衡</translation>
    </message>
    <message>
        <source>%1 profiles: %2</source>
        <translation>%1 个配置档: %2</translation>
    </message>
    <message>
        <source>, </source>
        <translation>, </translation>
    </message>
    <message>
        <source>No auto selector is running.</source>
        <translation>没有自动选择器正在运行。</translation>
    </message>
    <message>
        <source>That profile is not in the running pool.</source>
        <translation>该配置档不在运行池中。</translation>
    </message>
    <message>
        <source>Could not reach the core.</source>
        <translation>无法访问核心。</translation>
    </message>
    <message>
        <source>[Auto selector] Every profile is failing, but this machine has no network connection — keeping the current pool.</source>
        <translation>[自动选择器] 每个配置档都在失败，但是这个机器没有网络连接 — 保留当前的（配置）池。</translation>
    </message>
    <message>
        <source>Proxy: %1
Direct: %2</source>
        <translation>代理: %1
直连: %2</translation>
    </message>
    <message>
        <source>Proxy</source>
        <translation>代理</translation>
    </message>
    <message>
        <source>Other outbounds</source>
        <translation>其它出站</translation>
    </message>
    <message>
        <source>IPv4 forwarding breaks Tun mode</source>
        <translation>IPv4 转发导致 Tun 模式失效</translation>
    </message>
    <message>
        <source>IPv4 forwarding is on for the network adapter &quot;%1&quot;, usually because Mobile Hotspot or Internet Connection Sharing is sharing it.
Windows then ignores the adapter binding that keeps Throne&apos;s own connections out of the Tun, so they loop back into it and fail.
To fix this, share the hotspot from the throne-tun adapter instead of &quot;%1&quot; (Settings &gt; Mobile hotspot &gt; Share my internet connection from), or turn the hotspot off while using Tun mode.</source>
        <translation>IPv4 转发在网络适配器 &quot;%1&quot; 上开启，这通常是因为“移动热点”或“Internet 连接共享”功能共享它。
然后，导致 Windows 忽略旨在阻止 Throne 自身的连接进入 Tun 模式的适配器绑定，从而使流量回环到其本身而导致连接失败。
要修复这个问题，从 throne-tun 适配器而不是 &quot;%1&quot; 中共享热点 (设置&gt; 移动热点 &gt; 共享我的互联网连接来自)，或者在运行 Tun 模式时关闭热点。</translation>
    </message>
    <message>
        <source>Core exited, cleaning up...</source>
        <translation>核心已退出，正在清理...</translation>
    </message>
    <message>
        <source>Core exits too frequently, stop automatic restart this profile.</source>
        <translation>核心退出太频繁，停止自动重启这个配置档。</translation>
    </message>
    <message>
        <source>Restarting the core ...</source>
        <translation>正在重启核心...</translation>
    </message>
    <message>
        <source>None</source>
        <translation>无</translation>
    </message>
    <message>
        <source>Used: %1 Remain: %2 Expire: %3</source>
        <translation>已用: %1 剩余: %2 过期: %3</translation>
    </message>
    <message>
        <source>Auto-update: every %1</source>
        <translation>自动更新: 每隔 %1</translation>
    </message>
    <message>
        <source>Added remote routing profiles: %1 of %2 fetched</source>
        <translation>添加了远程路由配置档: 取得了 %1 / %2</translation>
    </message>
    <message>
        <source>Add profiles to this group</source>
        <translation>添加配置档到这个分组</translation>
    </message>
    <message>
        <source>Create new subscription group</source>
        <translation>创建新的订阅分组</translation>
    </message>
    <message>
        <source>Import HTTP proxy profile</source>
        <translation>导入 HTTP 代理配置档</translation>
    </message>
    <message>
        <source>url detected</source>
        <translation>检测到 URL</translation>
    </message>
    <message>
        <source>%1
How to update?</source>
        <translation>%1
如何更新？</translation>
    </message>
    <message>
        <source>Download Profiles</source>
        <translation>下载配置档</translation>
    </message>
    <message>
        <source>Requesting profile error: %1</source>
        <translation>请求配置档时出错: %1</translation>
    </message>
    <message>
        <source>Update</source>
        <translation>更新</translation>
    </message>
    <message>
        <source>Not official support platform</source>
        <translation>非官方支持平台</translation>
    </message>
    <message>
        <source>Requesting update error: %1</source>
        <translation>请求更新时出错：%1</translation>
    </message>
    <message>
        <source>No update</source>
        <translation>无更新</translation>
    </message>
    <message>
        <source>Update is ready, restart to install?</source>
        <translation>更新已准备好，重启进行安装吗？</translation>
    </message>
    <message>
        <source>a profile that no longer exists</source>
        <translation>不再存在的配置档</translation>
    </message>
    <message>
        <source>Select</source>
        <translation>选择</translation>
    </message>
    <message>
        <source>Download geo asset</source>
        <translation>下载 geo 资源</translation>
    </message>
    <message>
        <source>%1 was downloaded successfully.</source>
        <translation>%1 被成功下载。</translation>
    </message>
    <message>
        <source>Failed to download %1:
%2</source>
        <translation>下载 %1 失败:
%2</translation>
    </message>
    <message>
        <source>Too much data to fit in a QR code.</source>
        <translation>数据太多，无法放入二维码中。</translation>
    </message>
    <message>
        <source>Copy</source>
        <translation>复制</translation>
    </message>
    <message>
        <source>just now</source>
        <translation>刚刚</translation>
    </message>
    <message>
        <source>Working</source>
        <translation>运作中</translation>
    </message>
    <message>
        <source>Unstable</source>
        <translation>不稳定</translation>
    </message>
    <message>
        <source>Not checked</source>
        <translation>未检查</translation>
    </message>
    <message>
        <source>Failing</source>
        <translation>失败中</translation>
    </message>
    <message>
        <source>Paused</source>
        <translation>已暂停</translation>
    </message>
    <message>
        <source>your choice, carrying traffic now</source>
        <translation>你的选择，正在承载流量</translation>
    </message>
    <message>
        <source>your choice, but not usable right now</source>
        <translation>你的选择，但现在不可用</translation>
    </message>
    <message>
        <source>carrying traffic now</source>
        <translation>正在承载流量</translation>
    </message>
    <message>
        <source>failed to connect, retrying in %1s</source>
        <translation>连接失败，%1 秒后重试</translation>
    </message>
    <message>
        <source>failed to connect</source>
        <translation>连接失败</translation>
    </message>
    <message>
        <source>every check failed</source>
        <translation>每个检查失败</translation>
    </message>
    <message>
        <source>ready to take over</source>
        <translation>准备好接管</translation>
    </message>
    <message>
        <source>check in progress</source>
        <translation>检查进行中</translation>
    </message>
    <message>
        <source>queued for checking</source>
        <translation>已排队等待检查</translation>
    </message>
    <message>
        <source>%1 of %2 checks failed</source>
        <translation>%1 / %2 个检查失败</translation>
    </message>
    <message>
        <source>Connected</source>
        <translation>已连接</translation>
    </message>
    <message>
        <source>Connecting…</source>
        <translation>连接中…</translation>
    </message>
    <message>
        <source>Waiting for sign-in</source>
        <translation>正在等待登录</translation>
    </message>
    <message>
        <source>Error</source>
        <translation>错误</translation>
    </message>
    <message>
        <source>Unknown</source>
        <translation>未知</translation>
    </message>
    <message>
        <source>The schema is empty</source>
        <translation>模式(Schema)为空</translation>
    </message>
    <message>
        <source>The schema has no definition at %1</source>
        <translation>在 %1 处没有定义模式</translation>
    </message>
    <message>
        <source>Expected %1, got %2</source>
        <translation>预期 %1，实际 %2</translation>
    </message>
    <message>
        <source> or </source>
        <translation> 或 </translation>
    </message>
    <message>
        <source>Expected %1 here</source>
        <translation>这里预期为 %1</translation>
    </message>
    <message>
        <source>%1 is not valid here (expected: %2)</source>
        <translation>%1 在这里无效 (预期: %2)</translation>
    </message>
    <message>
        <source>This option is deprecated</source>
        <translation>这个选项已弃用</translation>
    </message>
    <message>
        <source>&quot;%1&quot; is not in the expected format</source>
        <translation>&quot;%1&quot; 不是采用预期的格式</translation>
    </message>
    <message>
        <source>Value must be at least %1</source>
        <translation>值必须至少为 %1</translation>
    </message>
    <message>
        <source>Value must be at most %1</source>
        <translation>值必须至多为 %1</translation>
    </message>
    <message>
        <source>Unknown field &quot;%1&quot;</source>
        <translation>未知字段 &quot;%1&quot;</translation>
    </message>
    <message>
        <source>Missing required field &quot;%1&quot;</source>
        <translation>缺少必需字段 &quot;%1&quot;</translation>
    </message>
    <message>
        <source>Unknown %1 %2 (expected: %3)</source>
        <translation>未知 %1 %2 (预期: %3)</translation>
    </message>
    <message>
        <source>Value does not match any accepted form</source>
        <translation>值不符合任何接受的形式</translation>
    </message>
    <message>
        <source>null</source>
        <translation>null</translation>
    </message>
    <message>
        <source>boolean</source>
        <translation>boolean</translation>
    </message>
    <message>
        <source>number</source>
        <translation>number</translation>
    </message>
    <message>
        <source>string</source>
        <translation>string</translation>
    </message>
    <message>
        <source>array</source>
        <translation>array</translation>
    </message>
    <message>
        <source>object</source>
        <translation>object</translation>
    </message>
    <message>
        <source>Empty document</source>
        <translation>空文档</translation>
    </message>
    <message>
        <source>Unexpected content after the end of the document</source>
        <translation>文档结尾后有意外内容</translation>
    </message>
    <message>
        <source>The document is nested too deeply</source>
        <translation>文档嵌套太深</translation>
    </message>
    <message>
        <source>Unexpected end of document</source>
        <translation>文档意外结束</translation>
    </message>
    <message>
        <source>Expected a property name in quotes</source>
        <translation>预期引号中的属性名</translation>
    </message>
    <message>
        <source>Expected a colon after the property name</source>
        <translation>预期在属性名称后有一个冒号</translation>
    </message>
    <message>
        <source>Trailing comma</source>
        <translation>尾随逗号</translation>
    </message>
    <message>
        <source>Expected a comma or a closing brace</source>
        <translation>预期出现逗号或右大括号</translation>
    </message>
    <message>
        <source>Expected a comma or a closing bracket</source>
        <translation>预期出现逗号或右括号</translation>
    </message>
    <message>
        <source>Unterminated string</source>
        <translation>未终止的字符串</translation>
    </message>
    <message>
        <source>Unterminated escape sequence</source>
        <translation>未终止的转义序列</translation>
    </message>
    <message>
        <source>Incomplete unicode escape sequence</source>
        <translation>不完整的 Unicode 转义序列</translation>
    </message>
    <message>
        <source>Invalid unicode escape sequence</source>
        <translation>无效的 Unicode 转义序列</translation>
    </message>
    <message>
        <source>Invalid escape sequence</source>
        <translation>无效的转义序列</translation>
    </message>
    <message>
        <source>Control character in string</source>
        <translation>字符串中的控制字符</translation>
    </message>
    <message>
        <source>Invalid value</source>
        <translation>无效值</translation>
    </message>
    <message>
        <source>Invalid number</source>
        <translation>无效数字</translation>
    </message>
    <message>
        <source>Database error</source>
        <translation>数据库错误</translation>
    </message>
    <message>
        <source>%1 failed: %2. Details are in the log file.</source>
        <translation>%1 失败： %2。详情在日志文件中。</translation>
    </message>
    <message>
        <source>Traffic statistics paused</source>
        <translation>流量统计已暂停</translation>
    </message>
    <message>
        <source>The statistics database is unusable (%1). Statistics are paused for this session and the file will be rebuilt when Throne restarts.</source>
        <translation>统计数据库不可用 (%1)。 本次会话将暂停统计功能，将在 Throne 重启时重建该文件。</translation>
    </message>
    <message>
        <source>Writing statistics keeps failing (%1). Statistics are paused until Throne restarts.</source>
        <translation>写入统计信息持续失败（%1）。统计功能将暂停，直至 Throne 重启。</translation>
    </message>
    <message>
        <source>Local</source>
        <translation>本地</translation>
    </message>
    <message>
        <source>Empty</source>
        <translation>空</translation>
    </message>
    <message numerus="yes">
        <source>%n problem(s)</source>
        <translation>
            <numerusform>%n 个问题</numerusform>
        </translation>
    </message>
    <message numerus="yes">
        <source>%n warning(s)</source>
        <translation>
            <numerusform>%n 个警告</numerusform>
        </translation>
    </message>
    <message>
        <source>Valid JSON</source>
        <translation>有效 JSON</translation>
    </message>
    <message>
        <source>JSON Editor</source>
        <translation>JSON 编辑器</translation>
    </message>
    <message>
        <source>Format</source>
        <translation>格式化</translation>
    </message>
    <message>
        <source>Invalid JSON</source>
        <translation>无效 JSON</translation>
    </message>
    <message>
        <source>Fix the errors before formatting the document.</source>
        <translation>在格式化文档前修复错误。</translation>
    </message>
    <message>
        <source>The document must be a JSON object.</source>
        <translation>文档必须是 JSON 对象。</translation>
    </message>
    <message>
        <source>Line %1: %2</source>
        <translation>第 %1 行: %2</translation>
    </message>
    <message>
        <source>%1: the private key is empty</source>
        <translation>%1: 私钥是空的</translation>
    </message>
    <message>
        <source>%1: no address is set</source>
        <translation>%1: 未设定地址</translation>
    </message>
    <message>
        <source>Settings</source>
        <translation>设置</translation>
    </message>
    <message>
        <source>%1 scan in progress %2/%3 (%4%)</source>
        <translation>%1 扫描进行中 %2/%3 (%4%)</translation>
    </message>
    <message>
        <source> · %1 found</source>
        <translation> · 找到 %1 项</translation>
    </message>
    <message>
        <source>+%1 more</source>
        <translation>还剩 %n 项</translation>
    </message>
    <message>
        <source>Settings changed, restart to apply</source>
        <translation>设置已更改，重启进行应用</translation>
    </message>
    <message>
        <source>Restart</source>
        <translation>重启</translation>
    </message>
    <message>
        <source>Ignore</source>
        <translation>忽略</translation>
    </message>
    <message>
        <source>Changed: %1</source>
        <translation>已更改: %1</translation>
    </message>
    <message>
        <source>Throne needs at least one group.</source>
        <translation>Throne 至少需要一个分组。</translation>
    </message>
    <message>
        <source>This group&apos;s subscription is being updated. Try again when it finishes.</source>
        <translation>这个分组的订阅正在被更新。当它完成时再试。</translation>
    </message>
    <message>
        <source>Close the editor of this group or of one of its profiles first.</source>
        <translation>先关闭这个分组的编辑器，或者它的其中一个配置档的编辑器。</translation>
    </message>
    <message>
        <source>This group no longer exists.</source>
        <translation>这个分组不再存在。</translation>
    </message>
    <message>
        <source>%1: the resolver &quot;%2&quot; is invalid: %3</source>
        <translation>%1: 解析器 &quot;%2&quot; 是无效的: %3</translation>
    </message>
    <message>
        <source>the resolver IP list no longer exists</source>
        <translation>解析器 IP 列表不再存在</translation>
    </message>
    <message>
        <source>the resolver IP list &quot;%1&quot; is empty</source>
        <translation>解析器 IP 列表 &quot;%1&quot; 是空的</translation>
    </message>
    <message>
        <source>the resolver IP list &quot;%1&quot; holds more than %2 entries; the rest were skipped</source>
        <translation>解析器 IP 列表“%1”的条目超过 %2 个；其余的已跳过</translation>
    </message>
    <message numerus="yes">
        <source>%n entries of the resolver IP list &quot;%1&quot; were skipped: ranges too large or past the resolver limit</source>
        <translation>
            <numerusform>解析器 IP 列表“%1”中的 %n 个条目已被跳过：范围过大或超出解析器数量限制</numerusform>
        </translation>
    </message>
    <message>
        <source>Unverified Host Key</source>
        <translation>未验证的主机钥匙</translation>
    </message>
    <message>
        <source>Enter the WebDAV server URL in the WebDAV settings.</source>
        <translation>在 WebDAV 设置中输入 WebDAV 服务器网址。</translation>
    </message>
    <message>
        <source>The WebDAV server URL must start with http:// or https://.</source>
        <translation>WebDAV 服务器 URL 必须以 http:// 或 https:// 开头。</translation>
    </message>
    <message>
        <source>The server sent more than %1 MB.</source>
        <translation>服务器已发送超过 %1 MB。</translation>
    </message>
    <message>
        <source>Could not write the downloaded backup: %1</source>
        <translation>无法写入下载的备份: %1</translation>
    </message>
    <message>
        <source>The server did not answer in time.</source>
        <translation>服务器没有及时应答。</translation>
    </message>
    <message>
        <source>The server could not be reached.</source>
        <translation>无法访问到服务器。</translation>
    </message>
    <message>
        <source>The server answered with a redirect (HTTP %1); check the server URL.</source>
        <translation>服务器回答了重定向 (HTTP %1)；检查服务器 URL。</translation>
    </message>
    <message>
        <source>The server redirected to %1; check the server URL.</source>
        <translation>服务器重定向到 %1；请检查服务器 URL。</translation>
    </message>
    <message>
        <source>Wrong username or password.</source>
        <translation>错误用户名或密码。</translation>
    </message>
    <message>
        <source>Permission denied.</source>
        <translation>权限被拒绝。</translation>
    </message>
    <message>
        <source>Not found.</source>
        <translation>找不到。</translation>
    </message>
    <message>
        <source>Server error (HTTP %1).</source>
        <translation>服务器错误 (HTTP %1)。</translation>
    </message>
    <message>
        <source>HTTP %1</source>
        <translation>HTTP %1</translation>
    </message>
    <message>
        <source>The server&apos;s folder listing could not be read.</source>
        <translation>无法读取服务器的文件夹列表。</translation>
    </message>
    <message>
        <source>Could not create the folder on the server: %1</source>
        <translation>无法创建文件夹于服务器上: %1</translation>
    </message>
    <message>
        <source>The backup list kept changing while it was being updated.</source>
        <translation>备份列表在其更新时一直在变化。</translation>
    </message>
    <message>
        <source>Could not read the backup file: %1</source>
        <translation>无法读取备份文件: %1</translation>
    </message>
    <message>
        <source>Unknown device</source>
        <translation>未知设备</translation>
    </message>
    <message>
        <source>The backup&apos;s file name is not safe to use.</source>
        <translation>备份的文件名不安全，不能使用。</translation>
    </message>
    <message>
        <source>Could not create a temporary file: %1</source>
        <translation>无法创建临时文件: %1</translation>
    </message>
    <message>
        <source>The downloaded backup does not match the backup list; it may have been replaced or damaged.</source>
        <translation>下载的备份与备份列表不匹配；它可能已被替换或损坏。</translation>
    </message>
    <message>
        <source>The built-in WARP MASQUE identity has not been generated yet (Routing Settings)</source>
        <translation>内置的 WARP MASQUE 身份还未生成（路由设置）</translation>
    </message>
    <message>
        <source>The built-in WARP WireGuard identity has not been generated yet (Routing Settings)</source>
        <translation>内置的 WARP WireGuard 身份尚未生成（路由设置）</translation>
    </message>
    <message>
        <source>Choose the DNS tunnel profile to find resolvers for</source>
        <translation>选择要为其查找解析器的 DNS 隧道配置档</translation>
    </message>
    <message>
        <source>Choose a profile for the config test</source>
        <translation>为配置测试选择一个配置档</translation>
    </message>
    <message>
        <source>Only DNSTT, MasterDNS and XDNS profiles can be used to find resolvers</source>
        <translation>仅 DNSTT、 MasterDNS 和 XDNS 配置档可用于查找解析器</translation>
    </message>
    <message>
        <source>This type of profile cannot be pointed at scanned addresses</source>
        <translation>这个类型的配置档无法指向扫描的地址</translation>
    </message>
    <message>
        <source>The profile for the config test could not be copied</source>
        <translation>用于配置测试的配置档无法被复制</translation>
    </message>
    <message>
        <source>Choose the WARP profile to take the identity from</source>
        <translation>选择要获取身份的 WARP 配置档</translation>
    </message>
    <message>
        <source>This mode needs a MASQUE profile</source>
        <translation></translation>
    </message>
    <message>
        <source>This mode needs a WireGuard profile</source>
        <translation>这个模式需要一个 WireGuard 配置档</translation>
    </message>
    <message>
        <source>Register a new identity for this mode</source>
        <translation>为这个模式注册一个新身份</translation>
    </message>
    <message>
        <source>The WARP identity could not be read; register a new one</source>
        <translation>无法读取 WARP 身份；注册一个新的</translation>
    </message>
    <message>
        <source>The routing profile sends traffic to %1, which holds an auto selector; such a chain can only be started, not used in routing rules</source>
        <translation>路由配置将流量发送到 %1，而它包含自动选择器；这样的链只能被启动，不能用于路由规则</translation>
    </message>
    <message numerus="yes">
        <source>[Auto selector] Left out %n profile(s) that cannot be reached through the hops in front of the selector (custom Xray configs, or too many switches between %1 and Xray).</source>
        <translation>
            <numerusform>[自动选择器] 已排除 %n 个无法经由选择器前方跳点访问的配置档（自定义 Xray 配置，或在 %1 与 Xray 之间切换次数过多）。</numerusform>
        </translation>
    </message>
    <message>
        <source>A chain can hold only one auto selector</source>
        <translation>一条链只能包含一个自动选择器</translation>
    </message>
    <message>
        <source>Extra Core profiles and custom configs cannot share a chain with an auto selector</source>
        <translation>「额外核心」配置档和自定义配置不能与自动选择器位于同一链中</translation>
    </message>
    <message>
        <source>Chains with an auto selector cannot be scanned</source>
        <translation>包含自动选择器的链无法扫描</translation>
    </message>
    <message>
        <source>The group&apos;s front or landing proxy no longer exists</source>
        <translation>分组的前端或落地代理已不存在</translation>
    </message>
    <message>
        <source>Chains with an auto selector cannot be tested hop by hop; test its members instead</source>
        <translation>包含自动选择器的链无法逐跳测试；请改为测试其成员</translation>
    </message>
    <message>
        <source>The group&apos;s front or landing proxy is not valid; the log has the details</source>
        <translation>分组的前端或落地代理无效；详情见日志</translation>
    </message>
    <message>
        <source>Auto selector paused — no network connection, or the chain hops in front of it are down (%1 profiles held)</source>
        <translation>自动选择器已暂停 — 没有网络连接，或其前方的链跳点已断开(保留了 %1 个配置档)</translation>
    </message>
    <message>
        <source>Filters of %1: %2</source>
        <translation>%1 的过滤器: %2</translation>
    </message>
    <message>
        <source>The filters of %1 remove all %2 profiles of the subscription, so the group was left unchanged.</source>
        <translation>%1 的过滤器会移除订阅的全部 %2 个配置档，因此分组保持不变。</translation>
    </message>
    <message>
        <source>Filtered out %1 profiles.</source>
        <translation>过滤掉了 %1 个配置档。</translation>
    </message>
    <message>
        <source>The country rules accept none of the %1 tested profiles, so none were removed.</source>
        <translation>国家规则不接受已测试的 %1 个配置档中的任何一个，因此没有移除任何配置档。</translation>
    </message>
    <message>
        <source>Removed %1 profiles outside the country rules:</source>
        <translation>移除了 %1 个不符合国家规则的配置档:</translation>
    </message>
    <message>
        <source>Testing the exit country of %1 profiles of %2 for its country rules. On a large group this takes a while.</source>
        <translation>正在为 %2 的国家规则测试 %1 个配置档的出口国家。分组较大时需要一段时间。</translation>
    </message>
    <message>
        <source>%1 profiles without a test result were kept; the next update tests them again.</source>
        <translation>%1 个没有测试结果的配置档被保留；下次更新时会再次测试。</translation>
    </message>
    <message>
        <source>After the country test of %1:</source>
        <translation>在 %1 进行国家测试后:</translation>
    </message>
    <message>
        <source>Service check, %1</source>
        <translation>服务检查，%1</translation>
    </message>
    <message>
        <source>Available</source>
        <translation>可用</translation>
    </message>
    <message>
        <source>Partial</source>
        <translation>部分可用</translation>
    </message>
    <message>
        <source>Restricted</source>
        <translation>受限</translation>
    </message>
    <message>
        <source>Blocked</source>
        <translation>已屏蔽</translation>
    </message>
    <message>
        <source>Failed</source>
        <translation>失败</translation>
    </message>
    <message>
        <source>IP</source>
        <translation>IP</translation>
    </message>
    <message>
        <source>risk %1</source>
        <translation>风险 %1</translation>
    </message>
    <message>
        <source>Choose the profile to find working settings for</source>
        <translation>选择要为其查找可用设置的配置档</translation>
    </message>
    <message>
        <source>The profile could not be copied</source>
        <translation>无法复制该配置档</translation>
    </message>
    <message>
        <source>Datacenter IP</source>
        <translation>数据中心 IP</translation>
    </message>
    <message>
        <source>Mobile IP</source>
        <translation>移动网络 IP</translation>
    </message>
    <message>
        <source>Business IP</source>
        <translation>企业 IP</translation>
    </message>
    <message>
        <source>Residential IP</source>
        <translation>家庭宽带 IP</translation>
    </message>
    <message>
        <source>listed as %1</source>
        <translation>被标记为 %1</translation>
    </message>
    <message>
        <source>Tor is enabled but not downloaded yet. Open Routing Settings &gt; Egress and press OK to download it.</source>
        <translation>Tor 已启用但尚未下载。打开“路由设置 &gt; 出口”并按“确定”即可下载。</translation>
    </message>
</context>
<context>
    <name>QPlatformTheme</name>
    <message>
        <source>OK</source>
        <translation>确定</translation>
    </message>
    <message>
        <source>Save</source>
        <translation>保存</translation>
    </message>
    <message>
        <source>Save All</source>
        <translation>全部保存</translation>
    </message>
    <message>
        <source>Open</source>
        <translation>打开</translation>
    </message>
    <message>
        <source>&amp;Yes</source>
        <translation>是(&amp;Y)</translation>
    </message>
    <message>
        <source>Yes to &amp;All</source>
        <translation>全部是(&amp;A)</translation>
    </message>
    <message>
        <source>&amp;No</source>
        <translation>否(&amp;N)</translation>
    </message>
    <message>
        <source>N&amp;o to All</source>
        <translation>全部否(&amp;O)</translation>
    </message>
    <message>
        <source>Abort</source>
        <translation>中止</translation>
    </message>
    <message>
        <source>Retry</source>
        <translation>重试</translation>
    </message>
    <message>
        <source>Ignore</source>
        <translation>忽略</translation>
    </message>
    <message>
        <source>Close</source>
        <translation>关闭</translation>
    </message>
    <message>
        <source>Cancel</source>
        <translation>取消</translation>
    </message>
    <message>
        <source>Discard</source>
        <translation>放弃</translation>
    </message>
    <message>
        <source>Help</source>
        <translation>帮助</translation>
    </message>
    <message>
        <source>Apply</source>
        <translation>应用</translation>
    </message>
    <message>
        <source>Reset</source>
        <translation>重置</translation>
    </message>
    <message>
        <source>Restore Defaults</source>
        <translation>恢复默认值</translation>
    </message>
</context>
<context>
    <name>Qv2ray::ui::widgets::AutoCompleteTextEdit</name>
    <message>
        <source>You can not input space characters here.</source>
        <translation>你不能在这里输入空格字符。</translation>
    </message>
</context>
<context>
    <name>RawRouteItem</name>
    <message>
        <source>Raw routing profile</source>
        <translation>原始路由配置档</translation>
    </message>
    <message>
        <source>Name</source>
        <translation>名称</translation>
    </message>
    <message>
        <source>Route</source>
        <translation>路由</translation>
    </message>
    <message>
        <source>DNS</source>
        <translation></translation>
    </message>
    <message>
        <source>Prevent modifications</source>
        <translation>防止修改</translation>
    </message>
    <message>
        <source>Use the route object exactly as written (outbound ids are still resolved to tags).
Throne will NOT add its DNS-hijack or xray bridge plumbing, so DNS, chained/xray
outbounds and other Throne features may break. For advanced users only.</source>
        <translation>按原样使用路由对象 (outbounds ids (出站标识符)仍被解析为标记)。
在 Throne 中「不会」添加它的 DNS-hijack (DNS劫持)或 xray bridge plumbing (桥接)等底层机制，
所以 DNS、链式/xray 出口和其它 Throne 功能可能会失效。仅限高级用户使用。</translation>
    </message>
    <message>
        <source>Format JSON</source>
        <translation>格式化 JSON</translation>
    </message>
    <message>
        <source>Raw route</source>
        <translation>原始路由</translation>
    </message>
    <message>
        <source>The route must be a valid JSON object</source>
        <translation>路由必须是一个有效的 JSON 对象</translation>
    </message>
    <message>
        <source>Invalid operation</source>
        <translation>无效操作</translation>
    </message>
    <message>
        <source>Cannot create Route Profile with empty name</source>
        <translation>无法创建带空名称的路由配置档</translation>
    </message>
    <message>
        <source>Invalid route</source>
        <translation>无效路由</translation>
    </message>
    <message numerus="yes">
        <source>The route has %n schema problem(s), and the core will likely refuse to start with it. Save anyway?</source>
        <translation>
            <numerusform>该路由有 %n 个模式问题，且核心很可能会拒绝用它启动。无论如何仍保存吗？</numerusform>
        </translation>
    </message>
</context>
<context>
    <name>RemoteApi</name>
    <message>
        <source>The request is not valid JSON: %1</source>
        <translation>该请求不是有效 JSON: %1</translation>
    </message>
    <message>
        <source>The request is not a JSON object</source>
        <translation>该请求不是 JSON 对象</translation>
    </message>
    <message>
        <source>The method must be GET, POST, PUT or DELETE</source>
        <translation>方法必须是 GET, POST, PUT 或 DELETE</translation>
    </message>
    <message>
        <source>The path must start with / and carry no query string or fragment</source>
        <translation>路径必须以 / 开始且不带查询字符串或片段</translation>
    </message>
    <message>
        <source>The query must be an object</source>
        <translation>查询必须是一个对象</translation>
    </message>
    <message>
        <source>Query value %1 is not a string</source>
        <translation>查询值 %1 不是字符串</translation>
    </message>
</context>
<context>
    <name>ResolverSourceWidget</name>
    <message>
        <source>IP list</source>
        <translation>IP 列表</translation>
    </message>
    <message>
        <source>Its addresses are used after the lines above; an entry without a port uses port 53.</source>
        <translation>其地址在以上各行之后使用；无端口的条目使用端口 53。</translation>
    </message>
    <message>
        <source>Find resolvers…</source>
        <translation>查找解析器...</translation>
    </message>
    <message>
        <source>One per line. Replaces the resolvers in Finalmask; empty keeps them.</source>
        <translation>每行一个。在 Finalmask 中替换解析器；留空则保留它们。</translation>
    </message>
    <message>
        <source>One per line. Empty uses the system resolver.</source>
        <translation>每行一个。留空则使用系统解析器。</translation>
    </message>
    <message>
        <source>None</source>
        <translation>无</translation>
    </message>
    <message>
        <source>Missing list</source>
        <translation>缺少的列表</translation>
    </message>
    <message>
        <source>Opens a scan that finds the resolvers relaying this profile&apos;s tunnel, as last saved.</source>
        <translation>打开扫描，查找能够转发这个配置档的隧道流量的解析器，按最近一次保存配置进行。</translation>
    </message>
    <message>
        <source>Save the profile first; the scan tests the saved profile.</source>
        <translation>先保存配置档；该扫描测试已保存的配置档。</translation>
    </message>
    <message>
        <source>Resolvers</source>
        <translation>解析器</translation>
    </message>
    <message>
        <source>This IP list no longer exists, so it adds no resolvers.</source>
        <translation>这个 IP 列表不再存在，因此未添加解析器。</translation>
    </message>
</context>
<context>
    <name>RouteDnsEditor</name>
    <message>
        <source>Use the global DNS settings</source>
        <translation>使用全局 DNS 设置</translation>
    </message>
    <message>
        <source>The servers set in Routing Settings &gt; DNS.</source>
        <translation>服务器在 “路由设置 &gt; DNS” 中设置。</translation>
    </message>
    <message>
        <source>Use custom DNS settings</source>
        <translation>使用自定义 DNS 设置</translation>
    </message>
    <message>
        <source>These servers replace the global ones while this routing profile is in use.</source>
        <translation>当使用这个路由配置档期间，这些服务器会替代全局服务器。</translation>
    </message>
    <message>
        <source>Routing Settings use a custom DNS object, so that object applies instead of the values below.</source>
        <translation>“路由设置”使用自定义 DNS 对象，所以会应用该对象而不是下面的值。</translation>
    </message>
    <message>
        <source>&lt;html&gt;&lt;head/&gt;&lt;body&gt;&lt;p&gt;examples:&lt;br/&gt;tls://8.8.8.8&lt;br/&gt;https://domain/path&lt;/p&gt;&lt;p&gt;tcp://8.8.8.8:1234&lt;/p&gt;&lt;p&gt;dhcp://auto&lt;/p&gt;&lt;p&gt;h3://domain/path&lt;/p&gt;&lt;p&gt;quic://domain:4632&lt;/p&gt;&lt;p&gt;localhost&lt;/p&gt;&lt;/body&gt;&lt;/html&gt;</source>
        <translation>&lt;html&gt;&lt;head/&gt;&lt;body&gt;&lt;p&gt;示例：&lt;br/&gt;tls://8.8.8.8&lt;br/&gt;https://domain/path&lt;/p&gt;&lt;p&gt;tcp://8.8.8.8:1234&lt;/p&gt;&lt;p&gt;dhcp://auto&lt;/p&gt;&lt;p&gt;h3://domain/path&lt;/p&gt;&lt;p&gt;quic://domain:4632&lt;/p&gt;&lt;p&gt;localhost&lt;/p&gt;&lt;/body&gt;&lt;/html&gt;</translation>
    </message>
    <message>
        <source>Remote DNS</source>
        <translation>远程 DNS</translation>
    </message>
    <message>
        <source>Disable IPv6</source>
        <translation>禁用 IPv6</translation>
    </message>
    <message>
        <source>Answer AAAA queries sent to this server with an empty result.</source>
        <translation>用空结果回答发送到这个服务器的 AAAA 查询。</translation>
    </message>
    <message>
        <source>Direct DNS</source>
        <translation>直连 DNS</translation>
    </message>
    <message>
        <source>Default DNS server</source>
        <translation>默认 DNS 服务器</translation>
    </message>
    <message>
        <source>Enable DNS Routing</source>
        <translation>启用 DNS 路由</translation>
    </message>
    <message>
        <source>Domains this profile sends direct are resolved with the direct DNS too.</source>
        <translation>这个配置档直接发送的域也会用直连 DNS 解析。</translation>
    </message>
    <message>
        <source>Enable FakeIP</source>
        <translation>启用 FakeIP</translation>
    </message>
    <message>
        <source>FakeIP Disable IPv6</source>
        <translation>FakeIP 禁用 IPv6</translation>
    </message>
    <message>
        <source>Hand out no fake IPv6 addresses; AAAA queries are answered empty.</source>
        <translation>不发出伪 IPv6 地址； AAAA 查询会被回答空结果。</translation>
    </message>
    <message>
        <source>Local Override, the hosts file, predefined answers and the advanced DNS settings always come from Routing Settings &gt; DNS.</source>
        <translation>“本地覆盖”、hosts 文件、预定义应答和高级 DNS 设置总是来自“路由设置 &gt; DNS”。</translation>
    </message>
</context>
<context>
    <name>RouteItem</name>
    <message>
        <source>Route Profile</source>
        <translation>路由配置档</translation>
    </message>
    <message>
        <source>General</source>
        <translation>常规</translation>
    </message>
    <message>
        <source>Name</source>
        <translation>名称</translation>
    </message>
    <message>
        <source>Default outbound</source>
        <translation>默认出站</translation>
    </message>
    <message>
        <source>Remote source</source>
        <translation>远程来源</translation>
    </message>
    <message>
        <source>URL</source>
        <translation>URL</translation>
    </message>
    <message>
        <source>https://... (throne://route link, base64, or JSON)</source>
        <translation>https://... (throne://路由链接, base64 或 JSON)</translation>
    </message>
    <message>
        <source>Re-fetch this profile from the URL when subscriptions auto-update.
Your local edits are overwritten on each update.</source>
        <translation>订阅自动更新时从该 URL 重新获取这个配置档。
每次更新时都会覆盖您的本地编辑。</translation>
    </message>
    <message>
        <source>Auto update</source>
        <translation>自动更新</translation>
    </message>
    <message>
        <source>Fetch the URL and show the routing profile without changing the current rules.</source>
        <translation>获取该 URL 并显示路由配置档，而不更改当前规则。</translation>
    </message>
    <message>
        <source>Preview</source>
        <translation>预览</translation>
    </message>
    <message>
        <source>Fetch the URL and replace the current rules with the fetched ones.</source>
        <translation>获取该 URL 并用取回的规则替换当前的。</translation>
    </message>
    <message>
        <source>Fetch</source>
        <translation>获取</translation>
    </message>
    <message>
        <source>Basic</source>
        <translation>基本</translation>
    </message>
    <message>
        <source>How to use</source>
        <translation>如何使用</translation>
    </message>
    <message>
        <source>Direct</source>
        <translation>直连</translation>
    </message>
    <message>
        <source>Proxy</source>
        <translation>代理</translation>
    </message>
    <message>
        <source>Block</source>
        <translation>阻止</translation>
    </message>
    <message>
        <source>Warp-bypass</source>
        <translation>Warp-bypass</translation>
    </message>
    <message>
        <source>Endpoints</source>
        <translation>端点</translation>
    </message>
    <message>
        <source>OpenVPN / OpenConnect profiles started alongside this routing profile. Traffic for the networks they advertise goes through them; everything else follows this profile&apos;s rules. Each endpoint gets a rule in the Advanced tab that you can move among your own rules. A chain endpoint can hand out one rule per inner OpenVPN / OpenConnect hop too, so traffic can be routed to a hop before the exit.</source>
        <translation>随这个路由配置档同步启动的 OpenVPN / OpenConnect 配置档。它们宣告的网络的流量将通过它们处理；其余的则遵循这个配置档的规则。每个端点都会在「高级」选项卡中得到一条规则，你可以在它们自有的规则中随意移动。链式端点也可以为每个内部 OpenVPN / OpenConnect 跳点分配一条规则，从而使流量可以在到达 Exit (最终出口)前被路由到某个（中间）跳点。</translation>
    </message>
    <message>
        <source>Add</source>
        <translation>添加</translation>
    </message>
    <message>
        <source>Remove</source>
        <translation>删除</translation>
    </message>
    <message>
        <source>Advanced</source>
        <translation>高级</translation>
    </message>
    <message>
        <source>Rules</source>
        <translation>规则</translation>
    </message>
    <message>
        <source>New</source>
        <translation>新建</translation>
    </message>
    <message>
        <source>Move Up</source>
        <translation>上移</translation>
    </message>
    <message>
        <source>Move Down</source>
        <translation>下移</translation>
    </message>
    <message>
        <source>Delete</source>
        <translation>删除</translation>
    </message>
    <message>
        <source>Rule Settings</source>
        <translation>规则设置</translation>
    </message>
    <message>
        <source>Action</source>
        <translation>动作</translation>
    </message>
    <message>
        <source>This rule is managed by the Endpoints tab. You can only move it.</source>
        <translation>这个规则由“端点”选项卡管理。只能调整其位置。</translation>
    </message>
    <message>
        <source>Disabled: kept in the profile but left out of the generated config.</source>
        <translation>禁用: 保留在配置档中，但排除在生成的配置之外。</translation>
    </message>
    <message>
        <source>Invalid rules</source>
        <translation>无效规则</translation>
    </message>
    <message>
        <source>Some rules could not be added:
</source>
        <translation>无法添加某些规则：
</translation>
    </message>
    <message>
        <source>Simple rule manual</source>
        <translation>简单规则手册</translation>
    </message>
    <message>
        <source>DNS</source>
        <translation></translation>
    </message>
    <message>
        <source>Invalid URL</source>
        <translation>无效 URL</translation>
    </message>
    <message>
        <source>Enter a valid http(s) URL first.</source>
        <translation>首先输入一个有效的 http(s) URL。</translation>
    </message>
    <message>
        <source>Fetch from remote</source>
        <translation>从远端获取</translation>
    </message>
    <message>
        <source>This will replace the current rules with the ones fetched from the URL. Continue?</source>
        <translation>这样将会用从该 URL 获取的规则来替换当前规则。 继续吗?</translation>
    </message>
    <message>
        <source>Fetching...</source>
        <translation>获取中…</translation>
    </message>
    <message>
        <source>Could not fetch routing profile</source>
        <translation>无法获取路由配置档</translation>
    </message>
    <message>
        <source>Loaded %1 rule(s) from the remote URL.</source>
        <translation>已从远程 URL 加载 %1 条规则。</translation>
    </message>
    <message>
        <source>Fetched</source>
        <translation>已获取</translation>
    </message>
    <message>
        <source>Fetched with warnings</source>
        <translation>已获取完成，但有警告</translation>
    </message>
    <message>
        <source>Remote routing profile preview</source>
        <translation>远程路由配置档预览</translation>
    </message>
    <message>
        <source>%1 — %2 rule(s)</source>
        <translation>%1 — %2 条规则</translation>
    </message>
    <message>
        <source>(unnamed)</source>
        <translation>(未命名)</translation>
    </message>
    <message>
        <source>Profile #%1 — deleted, dropped when you save</source>
        <translation>配置档 #%1 — 已删除，保存时丢弃</translation>
    </message>
    <message>
        <source>Allow routing to inner hops</source>
        <translation>允许路由到内部跳点</translation>
    </message>
    <message>
        <source>Give each OpenVPN/OpenConnect hop behind the exit its own rule, so traffic can be routed to it as well.</source>
        <translation>赋予出口节点(exit)后每个 OpenVPN/OpenConnect 跳点自己的规则，以便流量也可路由到这些跳点。</translation>
    </message>
    <message>
        <source>%1 route prefer</source>
        <translation>%1 路由首选</translation>
    </message>
    <message>
        <source>Some rules could not be added, fix them before saving:
</source>
        <translation>某些规则无法添加，请在保存前修复它们：
</translation>
    </message>
    <message>
        <source>Empty Route Profile</source>
        <translation>空路由配置档</translation>
    </message>
    <message>
        <source>No valid rules are in the profile</source>
        <translation>配置档中没有有效的规则</translation>
    </message>
    <message>
        <source>%1 endpoint profile(s) no longer exist and were removed from this routing profile.</source>
        <translation>%1 个端点配置档已不存在，已从这个路由配置档中移除。</translation>
    </message>
    <message>
        <source>Endpoint rule: move it to choose where the endpoint claims traffic. Managed by the Endpoints tab.</source>
        <translation>端点规则： 调整规则位置以选择该端点声明接管的流量的位置。由“端点”选项卡管理。</translation>
    </message>
    <message>
        <source>Missing profile (id %1)</source>
        <translation>缺少的配置档 (id %1)</translation>
    </message>
    <message>
        <source>Rule &quot;%1&quot; › %2</source>
        <translation>规则 &quot;%1&quot; › %2</translation>
    </message>
    <message>
        <source>Check attributes to show as tabs; unchecking clears their values.</source>
        <translation>勾选要显示为选项卡的属性；取消勾选则清除它们的值。</translation>
    </message>
    <message>
        <source>This rule installs a &apos;preferred by&apos; rule so that the networks advertised by the endpoint %1 get routed into the endpoint tunnel.</source>
        <translation>这个规则安装一个“优先使用”规则，以便由端点 %1 通告（播发）的网络被路由到该端点隧道。</translation>
    </message>
    <message>
        <source>Endpoint rule</source>
        <translation>端点规则</translation>
    </message>
    <message>
        <source>This rule belongs to &quot;%1&quot;, an inner hop of the endpoint &quot;%2&quot;.
Stop routing to that endpoint&apos;s inner hops?</source>
        <translation>这个规则属于 &quot;%1&quot;，它是端点 &quot;%2&quot; 的内部跳点。
停止路由到该端点的内部跳点吗?</translation>
    </message>
    <message>
        <source>This rule belongs to the endpoint &quot;%1&quot; and cannot be deleted on its own.
Remove that endpoint from this routing profile as well?</source>
        <translation>这条规则属于端点“%1”，且无法单独删除。
同时从这个路由配置档中移除该端点吗？</translation>
    </message>
</context>
<context>
    <name>Router</name>
    <message>
        <source>[API] %1 %2 from %3</source>
        <translation>[API] %1 %2 来自 %3</translation>
    </message>
    <message>
        <source>[API] %1</source>
        <translation>[API] %1</translation>
    </message>
    <message numerus="yes">
        <source>[API] Testing %n profile(s) to find the lowest latency</source>
        <translation>
            <numerusform>[API] 正在测试 %n 个配置档以查找最低延时</numerusform>
        </translation>
    </message>
    <message>
        <source>[API] Lowest latency: %1 (%2 ms)</source>
        <translation>[API] 最低延时: %1 (%2 毫秒)</translation>
    </message>
</context>
<context>
    <name>RuntimeStatsWidget</name>
    <message>
        <source>Process</source>
        <translation>进程</translation>
    </message>
    <message>
        <source>Core</source>
        <translation>核心</translation>
    </message>
    <message>
        <source>Running Config</source>
        <translation>正在运行的配置</translation>
    </message>
    <message>
        <source>Country</source>
        <translation>国别</translation>
    </message>
    <message>
        <source>Config</source>
        <translation>配置</translation>
    </message>
    <message>
        <source>Out IP</source>
        <translation>出口 IP</translation>
    </message>
    <message>
        <source>Ping</source>
        <translation>Ping</translation>
    </message>
    <message>
        <source>Endpoint</source>
        <translation>端点</translation>
    </message>
    <message>
        <source>State</source>
        <translation>状态</translation>
    </message>
    <message>
        <source>Network</source>
        <translation>网络</translation>
    </message>
    <message>
        <source>Connections</source>
        <translation>连接</translation>
    </message>
    <message>
        <source>Proxy</source>
        <translation>代理</translation>
    </message>
    <message>
        <source>Direct</source>
        <translation>直连</translation>
    </message>
    <message>
        <source>System</source>
        <translation>系统</translation>
    </message>
    <message>
        <source>Databases</source>
        <translation>数据库</translation>
    </message>
    <message>
        <source>Uptime</source>
        <translation>正常运行时间</translation>
    </message>
    <message>
        <source>Next sub update</source>
        <translation>下次订阅更新</translation>
    </message>
    <message>
        <source>Next remote route update</source>
        <translation>下次远程路由更新</translation>
    </message>
    <message>
        <source>CPU</source>
        <translation>CPU</translation>
    </message>
    <message>
        <source>RAM</source>
        <translation>内存</translation>
    </message>
    <message>
        <source>Disabled</source>
        <translation>禁用</translation>
    </message>
    <message>
        <source>Due now</source>
        <translation>到期</translation>
    </message>
    <message>
        <source>in %1</source>
        <translation>于 %1</translation>
    </message>
    <message>
        <source>Off</source>
        <translation>关闭</translation>
    </message>
    <message>
        <source>No active config</source>
        <translation>无活动配置</translation>
    </message>
    <message>
        <source>%1 active   ·   %2 TCP   ·   %3 UDP</source>
        <translation>活动: %1   ·   TCP: %2   ·   UDP: %3</translation>
    </message>
    <message>
        <source>Details</source>
        <translation>详情</translation>
    </message>
    <message>
        <source>Unavailable</source>
        <translation>不可用</translation>
    </message>
    <message>
        <source>N/A</source>
        <translation>不可用</translation>
    </message>
    <message>
        <source>Services</source>
        <translation>服务</translation>
    </message>
    <message>
        <source>Check</source>
        <translation>检查</translation>
    </message>
    <message>
        <source>Checking…</source>
        <translation>检测中…</translation>
    </message>
    <message>
        <source>Checked just now</source>
        <translation>刚刚检测</translation>
    </message>
    <message>
        <source>Checked %1 ago</source>
        <translation>%1前检测</translation>
    </message>
</context>
<context>
    <name>ScanItem</name>
    <message numerus="yes">
        <source>Starting over clears the %n result(s) this scan has found. Continue?</source>
        <translation>
            <numerusform>重新开始会清除这次扫描已找到的 %n 个结果。继续吗?</numerusform>
        </translation>
    </message>
    <message numerus="yes">
        <source>This scan was paused partway. Starting over discards its progress and clears its %n result(s). Continue?</source>
        <translation>
            <numerusform>这次扫描被中途暂停。重新开始将丢弃其进度并清除其 %n 个结果。继续吗？</numerusform>
        </translation>
    </message>
    <message>
        <source>This scan was paused partway. Starting over discards its progress. Continue?</source>
        <translation>这次扫描被中途暂停。重新开始将丢弃其进度。继续吗？</translation>
    </message>
    <message>
        <source>IP Scanner</source>
        <translation>IP 扫描器</translation>
    </message>
    <message numerus="yes">
        <source>The targets changed since this scan was paused, so it cannot resume. Start over from the beginning and clear its %n result(s)?</source>
        <translation>
            <numerusform>自从暂停这次扫描以来，目标已改变，所以无法继续。要重新从头开始并清除它的%n个结果吗？</numerusform>
        </translation>
    </message>
    <message>
        <source>The targets changed since this scan was paused, so it cannot resume. Start over from the beginning?</source>
        <translation>自从暂停这次扫描以来，目标已改变，所以无法继续。要重新从头开始吗？</translation>
    </message>
    <message>
        <source>The scan could not be started.</source>
        <translation>该扫描无法被启动。</translation>
    </message>
    <message>
        <source>Pausing…</source>
        <translation>正在暂停...</translation>
    </message>
    <message>
        <source>Running</source>
        <translation>正在运行</translation>
    </message>
    <message>
        <source>Idle</source>
        <translation>空闲</translation>
    </message>
    <message>
        <source>Paused</source>
        <translation>已暂停</translation>
    </message>
    <message>
        <source>Completed</source>
        <translation>已完成</translation>
    </message>
    <message>
        <source>Failed</source>
        <translation>失败</translation>
    </message>
    <message>
        <source>%1 / %2 (%3%)</source>
        <translation>%1 / %2 (%3%)</translation>
    </message>
    <message numerus="yes">
        <source>%n found</source>
        <translation>
            <numerusform>找到 %n 项</numerusform>
        </translation>
    </message>
    <message numerus="yes">
        <source>%n removed</source>
        <translation>
            <numerusform>移除 %n 项</numerusform>
        </translation>
    </message>
    <message>
        <source>WARP</source>
        <translation>WARP</translation>
    </message>
    <message>
        <source>DNS resolvers</source>
        <translation>DNS 解析器</translation>
    </message>
    <message>
        <source>Generic</source>
        <translation>通用</translation>
    </message>
    <message>
        <source>Only a saved DNSTT, MasterDNS or XDNS profile can be used to find resolvers; save the profile first.</source>
        <translation>仅已保存的 DNSTT、MasterDNS 或 XDNS 配置档才能用于查找解析器；请先保存配置档。</translation>
    </message>
    <message>
        <source>Resolvers for %1</source>
        <translation>%1 的解析器</translation>
    </message>
    <message>
        <source>Failed to store the scan.</source>
        <translation>存储该扫描失败。</translation>
    </message>
    <message>
        <source>Open</source>
        <translation>打开</translation>
    </message>
    <message>
        <source>Delete</source>
        <translation>删除</translation>
    </message>
    <message>
        <source>Pause</source>
        <translation>暂停</translation>
    </message>
    <message>
        <source>Resume</source>
        <translation>继续</translation>
    </message>
    <message>
        <source>Start</source>
        <translation>启动</translation>
    </message>
    <message>
        <source>&quot;%1&quot; cannot be searched. %2.</source>
        <translation>无法搜索“%1”。%2。</translation>
    </message>
    <message>
        <source>%1 / %2 attempts (%3%)</source>
        <translation>%1 / %2 次尝试 (%3%)</translation>
    </message>
    <message numerus="yes">
        <source>%n working variant(s)</source>
        <translation>
            <numerusform>%n 个可用变体</numerusform>
        </translation>
    </message>
    <message>
        <source>Find working settings</source>
        <translation>查找可用设置</translation>
    </message>
    <message>
        <source>Only a saved profile can be searched; save the profile first.</source>
        <translation>只能搜索已保存的配置档；请先保存该配置档。</translation>
    </message>
    <message>
        <source>Settings for %1</source>
        <translation>%1 的设置</translation>
    </message>
    <message numerus="yes">
        <source>Start over and discard the %n tested variant(s)?</source>
        <translation>
            <numerusform>重新开始并丢弃已测试的 %n 个变体吗？</numerusform>
        </translation>
    </message>
    <message>
        <source>The profile or the search options changed since this search was paused, so it cannot resume. Start over?</source>
        <translation>自暂停此搜索以来，配置档或搜索选项已更改，因此无法继续。要重新开始吗？</translation>
    </message>
    <message>
        <source>Working settings</source>
        <translation>可用设置</translation>
    </message>
</context>
<context>
    <name>ScanPhases</name>
    <message>
        <source>ICMP (ping)</source>
        <translation>ICMP (ping)</translation>
    </message>
    <message>
        <source>Timeout (ms)</source>
        <translation>超时 (毫秒)</translation>
    </message>
    <message>
        <source>Echo requests</source>
        <translation>回显请求</translation>
    </message>
    <message>
        <source>TCP connect</source>
        <translation>TCP 连接</translation>
    </message>
    <message>
        <source>Attempts</source>
        <translation>尝试次数</translation>
    </message>
    <message>
        <source>TLS / HTTP</source>
        <translation>TLS / HTTP</translation>
    </message>
    <message>
        <source>Use TLS</source>
        <translation>使用 TLS</translation>
    </message>
    <message>
        <source>SNI</source>
        <translation>SNI</translation>
    </message>
    <message>
        <source>The target address</source>
        <translation>目标地址</translation>
    </message>
    <message>
        <source>Host header</source>
        <translation>主机标头</translation>
    </message>
    <message>
        <source>The SNI, or the target address</source>
        <translation>SNI 或目标地址</translation>
    </message>
    <message>
        <source>Path</source>
        <translation>路径</translation>
    </message>
    <message>
        <source>Method</source>
        <translation>方法</translation>
    </message>
    <message>
        <source>Only a 200 OK answer passes.</source>
        <translation>仅 200 OK 应答算通过。</translation>
    </message>
    <message>
        <source>HTTP version</source>
        <translation>HTTP 版本</translation>
    </message>
    <message>
        <source>ALPN</source>
        <translation>ALPN</translation>
    </message>
    <message>
        <source>From the HTTP version</source>
        <translation>来自 HTTP 版本</translation>
    </message>
    <message>
        <source>Minimum TLS version</source>
        <translation>最低 TLS 版本</translation>
    </message>
    <message>
        <source>Maximum TLS version</source>
        <translation>最高 TLS 版本</translation>
    </message>
    <message>
        <source>uTLS fingerprint</source>
        <translation>uTLS 指纹</translation>
    </message>
    <message>
        <source>Allow insecure certificates</source>
        <translation>允许不安全证书</translation>
    </message>
    <message>
        <source>Disable SNI</source>
        <translation>禁用 SNI (不发送服务器名称指示)</translation>
    </message>
    <message>
        <source>Fragment the TLS handshake</source>
        <translation>分片 TLS 握手</translation>
    </message>
    <message>
        <source>Fragment fallback delay (ms)</source>
        <translation>分片回退延时 (毫秒)</translation>
    </message>
    <message>
        <source>Fragment TLS records</source>
        <translation>分片 TLS 记录</translation>
    </message>
    <message>
        <source>Mixed-case SNI</source>
        <translation>混合大小写 SNI</translation>
    </message>
    <message>
        <source>Config test</source>
        <translation>配置测试</translation>
    </message>
    <message>
        <source>Candidates that pass the probes are tested through a copy of this profile pointed at each of them.</source>
        <translation>通过探针的候选者会通过一个指向它们中每一个的这个配置档的副本进行测试。</translation>
    </message>
    <message>
        <source>Profile</source>
        <translation>配置档</translation>
    </message>
    <message>
        <source>Test URL</source>
        <translation>测试 URL</translation>
    </message>
    <message>
        <source>Report warm latency (two requests per target)</source>
        <translation>报告热机延时 (每个目标的两次请求)</translation>
    </message>
    <message>
        <source>WARP test</source>
        <translation>WARP 测试</translation>
    </message>
    <message>
        <source>Candidates that pass the probes are tested by connecting to WARP through them.</source>
        <translation>通过探测的候选项将通过经它们连接到 WARP 进行测试。</translation>
    </message>
    <message>
        <source>Mode</source>
        <translation>模式</translation>
    </message>
    <message>
        <source>MASQUE transport</source>
        <translation>MASQUE 传输</translation>
    </message>
    <message>
        <source>Identity</source>
        <translation>身份</translation>
    </message>
    <message>
        <source>Device</source>
        <translation>设备</translation>
    </message>
    <message>
        <source>Register</source>
        <translation>注册</translation>
    </message>
    <message>
        <source>MTU</source>
        <translation>MTU</translation>
    </message>
    <message>
        <source>QUIC opener</source>
        <translation>QUIC 前导包</translation>
    </message>
    <message>
        <source>Sends a decoy packet before each UDP flow to get past QUIC blocking.</source>
        <translation>在每个 UDP 流之前发送一个诱饵包，以绕过 QUIC 封锁。</translation>
    </message>
    <message>
        <source>Junk packets (Jc)</source>
        <translation>垃圾数据包 (Jc)</translation>
    </message>
    <message>
        <source>Junk minimum size (Jmin)</source>
        <translation>垃圾数据包最小大小 (Jmin)</translation>
    </message>
    <message>
        <source>Junk maximum size (Jmax)</source>
        <translation>垃圾数据包最大大小 (Jmax)</translation>
    </message>
    <message>
        <source>DNS tunnel test</source>
        <translation>DNS 隧道测试</translation>
    </message>
    <message>
        <source>Each address is tried as the only resolver of this profile; those that relay its tunnel are kept.</source>
        <translation>依次将每个地址作为这个配置档的唯一 DNS 解析器进行尝试；能够转发其隧道流量的地址会被保留。</translation>
    </message>
    <message>
        <source>Test</source>
        <translation>测试</translation>
    </message>
    <message>
        <source>Time limit per resolver. Empty: the test&apos;s default.</source>
        <translation>每个 DNS 解析器的时间限制。留空：使用测试的默认值。</translation>
    </message>
</context>
<context>
    <name>ScanTuneSettings</name>
    <message>
        <source>Profile</source>
        <translation>配置档</translation>
    </message>
    <message>
        <source>Variants of this profile&apos;s anti-censorship settings are tested through the core, one attempt at a time. The profile itself changes only when you apply a result.</source>
        <translation>此配置档的反审查设置的各个变体将通过核心逐次测试。只有在你应用结果时，配置档本身才会改变。</translation>
    </message>
    <message>
        <source>What to try</source>
        <translation>要尝试的项目</translation>
    </message>
    <message>
        <source>Each setting is first tried on its own, a switch flipped or a list value by value; the changes that help are then combined.</source>
        <translation>每项设置先单独尝试：开关翻转一次，列表则逐个值尝试；随后将有帮助的更改组合起来。</translation>
    </message>
    <message>
        <source>uTLS fingerprint</source>
        <translation>uTLS 指纹</translation>
    </message>
    <message>
        <source>Fingerprints to try</source>
        <translation>要尝试的指纹</translation>
    </message>
    <message>
        <source>TLS fragment</source>
        <translation>TLS 分片</translation>
    </message>
    <message>
        <source>Fallback delays to try</source>
        <translation>要尝试的回退延时</translation>
    </message>
    <message>
        <source>TLS record fragment</source>
        <translation>TLS 记录分片</translation>
    </message>
    <message>
        <source>Mixed-case SNI</source>
        <translation>混合大小写 SNI</translation>
    </message>
    <message>
        <source>QUIC opener</source>
        <translation>QUIC 前导包</translation>
    </message>
    <message>
        <source>MTU (WireGuard)</source>
        <translation>MTU (WireGuard)</translation>
    </message>
    <message>
        <source>Smallest MTU</source>
        <translation>最小 MTU</translation>
    </message>
    <message>
        <source>Largest MTU</source>
        <translation>最大 MTU</translation>
    </message>
    <message>
        <source>The profile&apos;s own</source>
        <translation>配置档自身的值</translation>
    </message>
    <message>
        <source>MTU step</source>
        <translation>MTU 步长</translation>
    </message>
    <message>
        <source>Download URL</source>
        <translation>下载 URL</translation>
    </message>
    <message>
        <source>Download size (bytes)</source>
        <translation>下载大小（字节）</translation>
    </message>
    <message>
        <source>Pacing</source>
        <translation>节奏</translation>
    </message>
    <message>
        <source>Attempts per variant</source>
        <translation>每个变体的尝试次数</translation>
    </message>
    <message>
        <source>More attempts tell a working variant from a lucky one.</source>
        <translation>尝试次数越多，越能区分真正可用的变体和侥幸成功的变体。</translation>
    </message>
    <message>
        <source>Attempts per minute</source>
        <translation>每分钟尝试次数</translation>
    </message>
    <message>
        <source>At most this many attempts start in any minute.</source>
        <translation>任意一分钟内最多开始这么多次尝试。</translation>
    </message>
    <message>
        <source>Attempt limit</source>
        <translation>尝试次数上限</translation>
    </message>
    <message>
        <source>The search stops after this many attempts in all.</source>
        <translation>总尝试次数达到此值后搜索停止。</translation>
    </message>
    <message>
        <source>Time limit (minutes)</source>
        <translation>时间上限（分钟）</translation>
    </message>
    <message>
        <source>The search stops after this many minutes.</source>
        <translation>搜索在这么多分钟后停止。</translation>
    </message>
    <message>
        <source>Test URL</source>
        <translation>测试 URL</translation>
    </message>
    <message>
        <source>Requested through the profile on every attempt.</source>
        <translation>每次尝试都通过该配置档请求。</translation>
    </message>
    <message>
        <source>Timeout (ms)</source>
        <translation>超时 (毫秒)</translation>
    </message>
</context>
<context>
    <name>Scanner::IpListUpdater</name>
    <message>
        <source>The IP list no longer exists</source>
        <translation>该 IP 列表不再存在</translation>
    </message>
    <message>
        <source>Only lists with a URL or rule-set source can be updated</source>
        <translation>仅带有 URL 或规则集源的列表可被更新</translation>
    </message>
    <message>
        <source>Updating IP list %1</source>
        <translation>正在更新 IP 列表 %1</translation>
    </message>
    <message>
        <source>Updating IP list %1 failed: %2</source>
        <translation>更新 IP 列表 %1 时失败: %2</translation>
    </message>
    <message>
        <source>its source changed during the update</source>
        <translation>其来源在更新期间更改</translation>
    </message>
    <message>
        <source>The source changed during the update</source>
        <translation>来源在更新期间被更改</translation>
    </message>
    <message>
        <source>The entries could not be saved</source>
        <translation>条目无法保存</translation>
    </message>
    <message numerus="yes">
        <source>IP list %1 updated (%n entries)</source>
        <translation>
            <numerusform>更新了 IP 列表 %1 (%n 项)</numerusform>
        </translation>
    </message>
    <message>
        <source>The list has no source to fetch</source>
        <translation>该列表没有要获取的来源</translation>
    </message>
    <message>
        <source>Unknown rule-set: %1</source>
        <translation>未知的规则集: %1</translation>
    </message>
    <message>
        <source>The source must be an http(s) URL</source>
        <translation>来源必须是一个 HTTP(S) URL</translation>
    </message>
    <message>
        <source>The core is not running</source>
        <translation>核心未在运行</translation>
    </message>
    <message>
        <source>The core could not read the rule-set</source>
        <translation>核心无法读取该规则集</translation>
    </message>
    <message>
        <source>The address returned a web page instead of an IP list</source>
        <translation>地址返回了网页而不是 IP 列表</translation>
    </message>
    <message>
        <source>No addresses found</source>
        <translation>找不到地址</translation>
    </message>
    <message>
        <source>IP list update failed: %1</source>
        <translation>IP 列表更新失败: %1</translation>
    </message>
    <message>
        <source>IP list update failed</source>
        <translation>IP 列表更新失败</translation>
    </message>
</context>
<context>
    <name>Scanner::ScanManager</name>
    <message>
        <source>The scan is already running</source>
        <translation>该扫描已在运行中</translation>
    </message>
    <message>
        <source>The scan no longer exists</source>
        <translation>该扫描不再存在</translation>
    </message>
    <message>
        <source>Turn on at least one test phase</source>
        <translation>开启至少一个测试阶段</translation>
    </message>
    <message>
        <source>A TLS handshake check needs TLS turned on</source>
        <translation>TLS 握手检查需要开启 TLS</translation>
    </message>
    <message>
        <source>HTTP/3 needs TLS turned on</source>
        <translation>HTTP/3 需要开启 TLS</translation>
    </message>
    <message>
        <source>The profile for the config test cannot be used</source>
        <translation>配置测试的配置档不能使用</translation>
    </message>
    <message>
        <source>Choose the IP list to scan</source>
        <translation>选择要扫描的 IP 列表</translation>
    </message>
    <message>
        <source>The IP list to scan is empty</source>
        <translation>要扫描的 IP 列表是空的</translation>
    </message>
    <message>
        <source>The result list is empty</source>
        <translation>结果列表是空的</translation>
    </message>
    <message>
        <source>Pause the scan first</source>
        <translation>先暂停该扫描</translation>
    </message>
    <message>
        <source>Scan %1 Result</source>
        <translation>扫描 %1 结果</translation>
    </message>
    <message>
        <source>config test</source>
        <translation>配置测试</translation>
    </message>
    <message>
        <source>Scanning %1 with %2</source>
        <translation>正在用 %2 扫描 %1</translation>
    </message>
    <message>
        <source>%1 failed %2: %3</source>
        <translation>%1 失败 %2: %3</translation>
    </message>
    <message>
        <source>Connected to %1 with TCP (%2 ms)</source>
        <translation>已用 TCP 连接到 %1 (%2 毫秒)</translation>
    </message>
    <message>
        <source>%1 replied to ping (%2 ms)</source>
        <translation>%1 回复 ping (%2 毫秒)</translation>
    </message>
    <message>
        <source>TLS handshake with %1 succeeded (%2 ms)</source>
        <translation>与 %1 TLS 握手成功 (%2 毫秒)</translation>
    </message>
    <message>
        <source>%1 answered over HTTP (%2 ms)</source>
        <translation>%1 通过 HTTP 应答 (%2 毫秒)</translation>
    </message>
    <message>
        <source>%1 passed the config test (%2 ms)</source>
        <translation>%1 通过了配置测试 (%2 毫秒)</translation>
    </message>
    <message>
        <source>%1 relays the DNS tunnel (%2 ms)</source>
        <translation>%1 转发 DNS 隧道 (%2 毫秒)</translation>
    </message>
    <message>
        <source>Preparing</source>
        <translation>正在准备</translation>
    </message>
    <message>
        <source>The scan stopped on an unexpected error</source>
        <translation>扫描在意外的错误处停止</translation>
    </message>
    <message>
        <source>Scan %1: saving its final state failed</source>
        <translation>扫描 %1: 保存其最终状态失败</translation>
    </message>
    <message>
        <source>Preparing targets…</source>
        <translation>正在准备目标...</translation>
    </message>
    <message>
        <source>The scan has no profile for the config test</source>
        <translation>该扫描没有用于配置测试的配置档</translation>
    </message>
    <message>
        <source>The result list could not be created</source>
        <translation>无法创建结果列表</translation>
    </message>
    <message>
        <source>The base IP list no longer exists</source>
        <translation>基础 IP 列表不再存在</translation>
    </message>
    <message>
        <source>The base IP list is empty</source>
        <translation>基础 IP 列表是空的</translation>
    </message>
    <message>
        <source>The base IP list holds only IPv6 ranges; enable IPv6 scanning</source>
        <translation>基础 IP 列表仅存有 IPv6 范围；启用 IPv6 扫描</translation>
    </message>
    <message>
        <source>%1 snapshot</source>
        <translation>%1 快照</translation>
    </message>
    <message>
        <source>The base IP list could not be copied</source>
        <translation>基础 IP 列表无法被复制</translation>
    </message>
    <message>
        <source>The base IP list holds no valid target</source>
        <translation>基础 IP 列表没有有效目标</translation>
    </message>
    <message>
        <source>The result list holds only IPv6 entries; enable IPv6 scanning</source>
        <translation>结果列表只有 IPv6 条目；启用 IPv6 扫描</translation>
    </message>
    <message>
        <source>%1 rescan snapshot</source>
        <translation>%1 重新扫描快照</translation>
    </message>
    <message>
        <source>The result list could not be copied</source>
        <translation>结果列表无法被复制</translation>
    </message>
    <message>
        <source>The result list holds no valid target</source>
        <translation>结果列表没有有效目标</translation>
    </message>
    <message>
        <source>The copy of the result list is gone; rescan the results again</source>
        <translation>结果列表的副本不见了；再次重新扫描结果</translation>
    </message>
    <message>
        <source>The copy of the base IP list is gone; restart the scan</source>
        <translation>基础 IP 列表的副本不见了；重新开始扫描</translation>
    </message>
    <message>
        <source>The DNS tunnel profile cannot be used to find resolvers</source>
        <translation>该 DNS 隧道配置档不能用于查找解析器</translation>
    </message>
    <message numerus="yes">
        <source>Found %n result(s); stopping as configured</source>
        <translation>
            <numerusform>找到 %n 个结果；已按配置停止中</numerusform>
        </translation>
    </message>
    <message>
        <source>IPv6 is unreachable from this network; scan paused (turn off IPv6 scanning to go on)</source>
        <translation>无法从这个网络访问 IPv6；扫描已暂停 (关闭 IPv6 扫描以继续)</translation>
    </message>
    <message>
        <source>Network appears to be down; scan paused</source>
        <translation>网络似乎中断了；扫描已暂停</translation>
    </message>
    <message>
        <source>Scan %1: network check failed: %2</source>
        <translation>扫描 %1: 网络检查失败: %2</translation>
    </message>
    <message>
        <source>IPv4 is unreachable from this network; IPv4 results were kept</source>
        <translation>从这个网络无法访问 IPv4；IPv4 结果已被保留</translation>
    </message>
    <message>
        <source>IPv6 is unreachable from this network; IPv6 results were kept</source>
        <translation>从这个网络无法访问 IPv6；IPv6 结果已被保留</translation>
    </message>
    <message>
        <source>The core ended the scan session early</source>
        <translation>核心提前结束了扫描会话</translation>
    </message>
    <message>
        <source>The core made no progress on the scan</source>
        <translation>核心在扫描上没有进展</translation>
    </message>
    <message numerus="yes">
        <source>%n result(s) passed only on a retry; the network fails some attempts at random</source>
        <translation>
            <numerusform>%n 个结果仅在重试时通过；网络某些尝试会随机失败</numerusform>
        </translation>
    </message>
    <message numerus="yes">
        <source>Re-testing %n failed result(s)…</source>
        <translation>
            <numerusform>正在重新测试 %n 个失败结果…</numerusform>
        </translation>
    </message>
    <message>
        <source>The core did not answer the probe request</source>
        <translation>核心不应答探针请求</translation>
    </message>
    <message numerus="yes">
        <source>Testing %n candidate(s) through the config…</source>
        <translation>
            <numerusform>正在通过配置测试 %n 个候选项…</numerusform>
        </translation>
    </message>
    <message>
        <source>The core did not answer the config test</source>
        <translation>核心不应答配置测试</translation>
    </message>
    <message>
        <source>Checking the network…</source>
        <translation>正在检查网络...</translation>
    </message>
    <message>
        <source>Scan %1 stopped: %2</source>
        <translation>扫描 %1 已停止: %2</translation>
    </message>
    <message numerus="yes">
        <source>Skipped %n invalid entry(ies)</source>
        <translation>
            <numerusform>跳过了 %n 个无效条目</numerusform>
        </translation>
    </message>
    <message>
        <source>Probing</source>
        <translation>正在探测</translation>
    </message>
    <message>
        <source>Testing configs</source>
        <translation>正在测试配置</translation>
    </message>
    <message>
        <source>Checking network</source>
        <translation>检查网络</translation>
    </message>
    <message>
        <source>A settings search has no results to rescan</source>
        <translation>设置搜索没有可重新扫描的结果</translation>
    </message>
    <message>
        <source>Choose the profile to find working settings for</source>
        <translation>选择要为其查找可用设置的配置档</translation>
    </message>
    <message>
        <source>Nothing to try: turn on a setting this profile supports</source>
        <translation>没有可尝试的项目：请开启一项此配置档支持的设置</translation>
    </message>
    <message>
        <source>The profile has no server address to test</source>
        <translation>该配置档没有可测试的服务器地址</translation>
    </message>
    <message>
        <source>Every variant connects to %1</source>
        <translation>每个变体都连接到 %1</translation>
    </message>
    <message numerus="yes">
        <source>Stopped at the time limit of %n minute(s)</source>
        <translation>
            <numerusform>已在 %n 分钟的时间上限处停止</numerusform>
        </translation>
    </message>
    <message numerus="yes">
        <source>Stopped at the limit of %n attempt(s)</source>
        <translation>
            <numerusform>已在 %n 次尝试的上限处停止</numerusform>
        </translation>
    </message>
    <message>
        <source>%1: cannot be used: %2</source>
        <translation>%1：无法使用：%2</translation>
    </message>
    <message>
        <source>%1: passed in %2 ms (%3/%4)</source>
        <translation>%1：通过，用时 %2 毫秒 (%3/%4)</translation>
    </message>
    <message>
        <source>The network is down, so the search paused; resume it when the connection is back</source>
        <translation>网络已断开，因此搜索已暂停；连接恢复后请继续</translation>
    </message>
    <message>
        <source>%1: failed (%2/%3): %4</source>
        <translation>%1：失败 (%2/%3)：%4</translation>
    </message>
    <message>
        <source>The profile could not be copied</source>
        <translation>无法复制该配置档</translation>
    </message>
    <message>
        <source>This profile cannot be pointed at %1</source>
        <translation>无法将此配置档指向 %1</translation>
    </message>
    <message>
        <source>The core returned no result</source>
        <translation>核心没有返回结果</translation>
    </message>
    <message>
        <source>The core did not answer the network check, so the search paused</source>
        <translation>核心没有响应网络检查，因此搜索已暂停</translation>
    </message>
    <message>
        <source>Confirming the best variants</source>
        <translation>正在确认最佳变体</translation>
    </message>
    <message>
        <source>Testing the profile as it is</source>
        <translation>正在测试原样的配置档</translation>
    </message>
    <message>
        <source>Trying one change at a time</source>
        <translation>正在逐项尝试更改</translation>
    </message>
    <message>
        <source>Combining what helped</source>
        <translation>正在组合有效的更改</translation>
    </message>
    <message>
        <source>Trying fragment delays</source>
        <translation>正在尝试分片延迟</translation>
    </message>
    <message>
        <source>Searching for the largest working MTU</source>
        <translation>正在搜索可用的最大 MTU</translation>
    </message>
</context>
<context>
    <name>Scanner::Tune</name>
    <message>
        <source>The profile could not be read</source>
        <translation>无法读取该配置档</translation>
    </message>
    <message>
        <source>A chain cannot be searched as a whole; run the search on the profiles of its hops</source>
        <translation>无法对整条链进行搜索；请对其各跳点的配置档分别运行搜索</translation>
    </message>
    <message>
        <source>Xray profiles are not supported yet: the search changes sing-box settings</source>
        <translation>暂不支持 Xray 配置档：搜索更改的是 sing-box 设置</translation>
    </message>
    <message>
        <source>This profile has no setting the search can change: that needs TLS over TCP, QUIC or WireGuard</source>
        <translation>此配置档没有可供搜索更改的设置：需要基于 TCP 的 TLS、QUIC 或 WireGuard</translation>
    </message>
    <message>
        <source>The MTU applies only to WireGuard profiles</source>
        <translation>MTU 仅适用于 WireGuard 配置档</translation>
    </message>
    <message>
        <source>The profile no longer uses TLS over TCP</source>
        <translation>该配置档不再使用基于 TCP 的 TLS</translation>
    </message>
    <message>
        <source>This type of profile has no uTLS fingerprint</source>
        <translation>此类型的配置档没有 uTLS 指纹</translation>
    </message>
    <message>
        <source>Unknown setting “%1”</source>
        <translation>未知设置“%1”</translation>
    </message>
    <message>
        <source>The profile as it is</source>
        <translation>原样的配置档</translation>
    </message>
    <message>
        <source>uTLS off</source>
        <translation>关闭 uTLS</translation>
    </message>
    <message>
        <source>Fingerprint %1</source>
        <translation>指纹 %1</translation>
    </message>
    <message>
        <source>TLS fragment on</source>
        <translation>TLS 分片开启</translation>
    </message>
    <message>
        <source>TLS fragment off</source>
        <translation>TLS 分片关闭</translation>
    </message>
    <message>
        <source>Fragment delay %1</source>
        <translation>分片延迟 %1</translation>
    </message>
    <message>
        <source>default</source>
        <translation>默认</translation>
    </message>
    <message>
        <source>Record fragment on</source>
        <translation>记录分片开启</translation>
    </message>
    <message>
        <source>Record fragment off</source>
        <translation>记录分片关闭</translation>
    </message>
    <message>
        <source>Mixed-case SNI on</source>
        <translation>混合大小写 SNI 开启</translation>
    </message>
    <message>
        <source>Mixed-case SNI off</source>
        <translation>混合大小写 SNI 关闭</translation>
    </message>
    <message>
        <source>QUIC opener on</source>
        <translation>QUIC 前导包开启</translation>
    </message>
    <message>
        <source>QUIC opener off</source>
        <translation>QUIC 前导包关闭</translation>
    </message>
    <message>
        <source>MTU %1</source>
        <translation>MTU %1</translation>
    </message>
    <message>
        <source>“%1” changes from one start to the next, so it cannot be tried</source>
        <translation>“%1”每次启动都会变化，因此无法尝试</translation>
    </message>
    <message>
        <source>“%1” is not a uTLS fingerprint</source>
        <translation>“%1”不是 uTLS 指纹</translation>
    </message>
    <message>
        <source>Fragment delay “%1”: %2</source>
        <translation>分片延迟“%1”：%2</translation>
    </message>
    <message>
        <source>The largest MTU must be above the smallest</source>
        <translation>最大 MTU 必须大于最小 MTU</translation>
    </message>
</context>
<context>
    <name>Server</name>
    <message>
        <source>The request was dropped without an answer</source>
        <translation>该请求被丢弃，没有应答</translation>
    </message>
    <message>
        <source>Off</source>
        <translation>关闭</translation>
    </message>
    <message>
        <source>Not listening: the key is missing or invalid</source>
        <translation>无监听: 缺少钥匙或钥匙无效</translation>
    </message>
    <message>
        <source>Port %1 is in use</source>
        <translation>端口 %1 在使用中</translation>
    </message>
    <message>
        <source>Cannot listen on port %1: %2</source>
        <translation>无法监听端口 %1: %2</translation>
    </message>
    <message>
        <source>Listening on port %1 (local network)</source>
        <translation>正在监听端口 %1 (本地网络)</translation>
    </message>
    <message>
        <source>Listening on %1:%2</source>
        <translation>正在监听 %1:%2</translation>
    </message>
    <message>
        <source>Stopped listening</source>
        <translation>已停止监听</translation>
    </message>
    <message>
        <source>Refused %1: too many failed attempts</source>
        <translation>已拒绝 %1: 失败尝试次数太多</translation>
    </message>
    <message>
        <source>Refused %1: not in the allowed addresses</source>
        <translation>已拒绝 %1: 不在允许的地址中</translation>
    </message>
    <message>
        <source>Refused %1: too many connections</source>
        <translation>已拒绝 %1: 太多连接</translation>
    </message>
    <message>
        <source>unsupported protocol version</source>
        <translation>不支持的协议版本</translation>
    </message>
    <message>
        <source>invalid frame length</source>
        <translation>无效的帧长度</translation>
    </message>
    <message>
        <source>a frame failed authentication, most likely the key is wrong</source>
        <translation>帧认证失败，很可能是钥匙错误</translation>
    </message>
    <message>
        <source>The response is too large</source>
        <translation>响应太大</translation>
    </message>
    <message>
        <source>Closed the connection from %1: %2</source>
        <translation>已关闭来自 %1 的连接: %2</translation>
    </message>
    <message>
        <source>Throne is shutting down</source>
        <translation>Throne 正在关机</translation>
    </message>
</context>
<context>
    <name>StartStopButton</name>
    <message>
        <source>Select a profile to start</source>
        <translation>选择要启动的一个配置档</translation>
    </message>
    <message>
        <source>Start</source>
        <translation>启动</translation>
    </message>
    <message>
        <source>Connecting…</source>
        <translation>连接中…</translation>
    </message>
    <message>
        <source>Stop</source>
        <translation>停止</translation>
    </message>
    <message>
        <source>Stopping…</source>
        <translation>正在停止…</translation>
    </message>
    <message>
        <source>Kill switch: starting…</source>
        <translation>急停开关: 正在启动...</translation>
    </message>
    <message>
        <source>Kill switch: Tun is not running, so most traffic is blocked</source>
        <translation>急停开关: Tun 未运行，所以大多数流量被拦截</translation>
    </message>
    <message>
        <source>Kill switch: on — traffic only flows through Throne&apos;s Tun</source>
        <translation>急停开关: 开启 - 流量仅流经 Throne 的 Tun</translation>
    </message>
    <message>
        <source>Kill switch is blocking traffic</source>
        <translation>急停开关正在拦截流量</translation>
    </message>
    <message>
        <source>Kill switch is not active</source>
        <translation>急停开关未激活</translation>
    </message>
</context>
<context>
    <name>SubscriptionInfoCard</name>
    <message>
        <source>Portal</source>
        <translation>门户</translation>
    </message>
    <message>
        <source>Support</source>
        <translation>支持</translation>
    </message>
    <message>
        <source>Expired</source>
        <translation>过期</translation>
    </message>
    <message>
        <source>%1d %2h left</source>
        <translation>剩下 %1 天 %2 小时</translation>
    </message>
    <message>
        <source>%1h %2m left</source>
        <translation>剩下 %1 小时 %2 分</translation>
    </message>
    <message>
        <source>%1m left</source>
        <translation>剩下 %1 分</translation>
    </message>
    <message>
        <source>Announcement - %1</source>
        <translation>通告 - %1</translation>
    </message>
    <message>
        <source>Auto-update: every %1</source>
        <translation>自动更新: 每隔 %1</translation>
    </message>
    <message>
        <source>Provider: %1</source>
        <translation>提供方: %1</translation>
    </message>
    <message>
        <source>Last updated: %1</source>
        <translation>上次更新: %1</translation>
    </message>
    <message>
        <source>Expires: %1</source>
        <translation>过期: %1</translation>
    </message>
    <message>
        <source>Interval provided by the server</source>
        <translation>由服务器提供的间隔</translation>
    </message>
    <message>
        <source>Interval set for this group</source>
        <translation>为这个分组设置的间隔</translation>
    </message>
    <message>
        <source>Global interval from Settings</source>
        <translation>来自设置的全局间隔</translation>
    </message>
    <message numerus="yes">
        <source>Server suggests every %n hour(s)</source>
        <translation>
            <numerusform>服务器建议每 %n 小时</numerusform>
        </translation>
    </message>
    <message>
        <source>Every %1</source>
        <translation>每隔 %1</translation>
    </message>
    <message>
        <source>Website / Portal: %1</source>
        <translation>网站 / 门户: %1</translation>
    </message>
    <message>
        <source>Technical Support: %1</source>
        <translation>技术支持: %1</translation>
    </message>
</context>
<context>
    <name>Sys::CoreDiagnostics</name>
    <message>
        <source>The core is not running.</source>
        <translation>核心未在运行。</translation>
    </message>
    <message>
        <source>Could not reach the core. See the log for details.</source>
        <translation>无法访问核心。查看日志了解详情。</translation>
    </message>
    <message>
        <source>The core could not capture the profile: %1</source>
        <translation>核心无法捕获剖析文件: %1</translation>
    </message>
    <message>
        <source>The core returned an empty profile.</source>
        <translation>该核心返回了一个空剖析文件。</translation>
    </message>
    <message>
        <source>Could not save %1: %2</source>
        <translation>无法保存 %1: %2</translation>
    </message>
    <message>
        <source>Performance profile saved to %1</source>
        <translation>性能剖析数据已保存到 %1</translation>
    </message>
    <message>
        <source>The performance profile was saved (%1).</source>
        <translation>性能剖析已保存 (%1)。</translation>
    </message>
    <message>
        <source>Performance profile failed: %1</source>
        <translation>性能剖析失败: %1</translation>
    </message>
    <message>
        <source>Performance profile</source>
        <translation>性能剖析</translation>
    </message>
    <message>
        <source>Show in folder</source>
        <translation>在文件夹中显示</translation>
    </message>
</context>
<context>
    <name>Sys::KillSwitch</name>
    <message>
        <source>The kill switch needs the core to run as administrator.</source>
        <translation>急停开关需要核心以管理员身份运行。</translation>
    </message>
    <message>
        <source>The kill switch needs the core to have root privileges.</source>
        <translation>急停开关需要核心拥有 root 权限。</translation>
    </message>
    <message>
        <source>The kill switch is not supported on this system.</source>
        <translation>在这个系统上不支持急停开关。</translation>
    </message>
    <message>
        <source>Another program removed the kill switch rules or disabled the system firewall.</source>
        <translation>另一个程序移除了该急停开关规则或禁用了系统防火墙。</translation>
    </message>
    <message>
        <source>The kill switch rejected its settings.</source>
        <translation>急停开关拒绝了其设置。</translation>
    </message>
    <message>
        <source>The kill switch process stopped unexpectedly.</source>
        <translation>急停开关进程异常停止。</translation>
    </message>
    <message>
        <source>Details: %1</source>
        <translation>详情: %1</translation>
    </message>
    <message>
        <source>[Kill switch] Off.</source>
        <translation>[急停开关] 关闭。</translation>
    </message>
    <message>
        <source>[Kill switch] Allowing the extra core %1.</source>
        <translation>[急停开关] 允许附加核心 %1。</translation>
    </message>
    <message>
        <source>[Kill switch] Warning: %1</source>
        <translation>[急停开关] 警告: %1</translation>
    </message>
    <message>
        <source>[Kill switch] On: only traffic through Throne can leave this device.</source>
        <translation>[急停开关] 开启: 只有通过 Throne 的流量才能离开这个设备。</translation>
    </message>
    <message>
        <source>[Kill switch] New settings applied.</source>
        <translation>[急停开关] 应用了新设置。</translation>
    </message>
    <message>
        <source>the kill switch did not respond within %1 seconds</source>
        <translation>急停开关在 %1 秒内不响应</translation>
    </message>
    <message>
        <source>could not start %1: %2</source>
        <translation>无法启动 %1: %2</translation>
    </message>
    <message>
        <source>the kill switch process crashed</source>
        <translation>急停开关进程崩溃</translation>
    </message>
    <message>
        <source>the kill switch process exited with code %1</source>
        <translation>急停开关进程以代码 %1 退出</translation>
    </message>
    <message>
        <source>the kill switch process crashed before it was ready</source>
        <translation>急停开关在其准备就绪前崩溃</translation>
    </message>
    <message>
        <source>the kill switch process exited with code %1 before it was ready</source>
        <translation>急停开关进程在其准备就绪前以代码 %1 退出</translation>
    </message>
    <message>
        <source>[Kill switch] The new settings could not be applied, the previous ones stay in force: %1</source>
        <translation>[急停开关] 新设置无法应用，先前的设置保持强制有效: %1</translation>
    </message>
    <message>
        <source>[Kill switch] Protection lost: %1</source>
        <translation>[急停开关] 保护丢失: %1</translation>
    </message>
    <message>
        <source>[Kill switch] Starting it again...</source>
        <translation>[急停开关] 正在再次启动...</translation>
    </message>
    <message>
        <source>[Kill switch] Not active: %1</source>
        <translation>[急停开关] 未激活: %1</translation>
    </message>
</context>
<context>
    <name>TrafficChartWidget</name>
    <message>
        <source>No traffic recorded for this period</source>
        <translation>这个周期无流量记录</translation>
    </message>
    <message>
        <source>Download</source>
        <translation>下载(↓)</translation>
    </message>
    <message>
        <source>Upload</source>
        <translation>上传(↑)</translation>
    </message>
</context>
<context>
    <name>TrayOtpCodes</name>
    <message>
        <source>No OTP profiles yet</source>
        <translation>尚无 OTP 配置档</translation>
    </message>
    <message>
        <source>No matches</source>
        <translation>无匹配</translation>
    </message>
    <message>
        <source>Invalid secret</source>
        <translation>无效密钥</translation>
    </message>
    <message>
        <source>Copied</source>
        <translation>已复制</translation>
    </message>
</context>
<context>
    <name>TrayPopupFrame</name>
    <message>
        <source>Search…</source>
        <translation>搜索...</translation>
    </message>
    <message>
        <source>Close</source>
        <translation>关闭</translation>
    </message>
</context>
<context>
    <name>TrayProfileSelector</name>
    <message>
        <source>Back to groups</source>
        <translation>返回分组</translation>
    </message>
    <message>
        <source>Select Routing</source>
        <translation>选择路由</translation>
    </message>
    <message>
        <source>No matches</source>
        <translation>无匹配</translation>
    </message>
    <message>
        <source>No routing profiles</source>
        <translation>无路由配置档</translation>
    </message>
    <message>
        <source>Select Server</source>
        <translation>选择服务器</translation>
    </message>
    <message>
        <source>Stop: %1</source>
        <translation>停止: %1</translation>
    </message>
    <message>
        <source>No servers</source>
        <translation>无服务器</translation>
    </message>
    <message>
        <source>Page %1/%2</source>
        <translation>页面 %1/%2</translation>
    </message>
</context>
<context>
    <name>Validation</name>
    <message>
        <source>Add at least one profile to the chain.</source>
        <translation>添加至少一个配置档到链。</translation>
    </message>
    <message>
        <source>A quote is not closed; end it with the same quote mark.</source>
        <translation>引号没有闭合；用相同的引号结束它。</translation>
    </message>
    <message>
        <source>The arguments end with a backslash; remove it or add the character it escapes.</source>
        <translation>该参数以反斜杠结尾；删除它或添加它转义的字符。</translation>
    </message>
    <message>
        <source>Add %s to the arguments where the config file path goes; without it the config is not used.</source>
        <translation>添加 %s 到代表配置文件路径所在位置的参数；没有它则不会使用配置。</translation>
    </message>
    <message>
        <source>These dial options are not supported and were ignored: %1.</source>
        <translation>这些拨号选项不被支持，已被忽略: %1.</translation>
    </message>
    <message>
        <source>To reach this server through another one, build a chain profile instead of a detour.</source>
        <translation>要通过另一个服务器访问这个服务器，请建立一个链式配置档来代替绕行(detour)。</translation>
    </message>
    <message>
        <source>TCP Brutal only works with a custom multiplex. Set Multiplex to Custom.</source>
        <translation>TCP Brutal 仅配合自定义多路复用工作。设置「多路复用」为“自定义(Custom)”。</translation>
    </message>
    <message>
        <source>TCP Brutal needs both speeds; a missing one is sent as 1 Mbps.</source>
        <translation>TCP Brutal 需要两种速度；缺少的一种按 1Mbps 发送。</translation>
    </message>
    <message>
        <source>The largest fragment must be at least 1 byte, for example 10-100.</source>
        <translation>最大片段必须至少 1 字节，例如 10-100。</translation>
    </message>
    <message>
        <source>Fragments above 400 bytes can fail the connection when the ClientHello is very large; a maximum of 400 or less is always safe.</source>
        <translation>当 ClientHello 非常大时，超过 400 字节的片段可能会导致连接失败；最大为 400 或更少总是安全的。</translation>
    </message>
    <message>
        <source>The TLS fragment size &quot;%1&quot; in Preset Settings &gt; TLS is invalid. %2</source>
        <translation>“预设设置 &gt;TLS”中的 TLS 分片大小“%1”无效。 %2</translation>
    </message>
    <message>
        <source>The TLS fragment sleep &quot;%1&quot; in Preset Settings &gt; TLS is invalid. %2</source>
        <translation>“预设设置 &gt;TLS”中的 TLS 分片睡眠“%1”无效。 %2</translation>
    </message>
    <message>
        <source>The server address is missing.</source>
        <translation>缺少服务器地址。</translation>
    </message>
    <message>
        <source>The server port is missing.</source>
        <translation>缺少服务器端口。</translation>
    </message>
    <message>
        <source>Refused to import an Extra Core profile: it would run a program on this computer.</source>
        <translation>拒绝导入“额外核心”配置档：它会在这台计算机上运行一个程序。</translation>
    </message>
    <message>
        <source>Refused to import a Chain Proxy profile: its hops are profiles of another installation.</source>
        <translation>拒绝导入链式代理配置档：其跳转是其他安装的配置档。</translation>
    </message>
    <message>
        <source>Refused to import an Auto Selector profile: the group it tracks belongs to another installation.</source>
        <translation>拒绝导入自动选择器配置文件：它所跟踪的分组属于另一个安装。</translation>
    </message>
    <message>
        <source>The receive window must be a whole number of bytes or a size such as 8m, so it was ignored.</source>
        <translation>接收窗口必须是整字节数或像 8m 这样的大小，因此它被忽略了。</translation>
    </message>
    <message>
        <source>Must be 0 (the default) or from 1200 to 1452.</source>
        <translation>必须为 0（默认值）或从 1200 到 1452 之间。</translation>
    </message>
    <message>
        <source>Each pin must be a SHA-256 in hexadecimal (64 characters, colons allowed).</source>
        <translation>每个 pin 必须是十六进制的 SHA-256（64 个字符，允许使用冒号）。</translation>
    </message>
    <message>
        <source>sing-box refuses this suite because Go lists it as insecure (CBC-SHA256, RC4, 3DES and RSA key exchange); only VLESS profiles running on Xray can use it.</source>
        <translation>sing-box 拒绝这个套件，因为 Go 将其列为不安全（CBC-SHA256、RC4、3DES 和 RSA 钥匙交换）；只有在 Xray 上运行的 VLESS 配置档可以使用它。</translation>
    </message>
    <message>
        <source>Unknown cipher suite. Use Go&apos;s names, for example TLS_AES_128_GCM_SHA256.</source>
        <translation>未知的密码套件。使用 Go 的名称，例如 TLS_AES_128_GCM_SHA256。</translation>
    </message>
    <message>
        <source>Unknown fingerprint &quot;%1&quot;. Use chrome, firefox, safari, ios, android, edge, 360, qq, random, randomized or unsafe.</source>
        <translation>未知指纹 &quot;%1&quot;。请使用 chrome、firefox、safari、ios、android、edge、360、qq、random、randomized 或 unsafe。</translation>
    </message>
    <message>
        <source>The minimum TLS version is above the maximum.</source>
        <translation>最小 TLS 版本高于最大的版本。</translation>
    </message>
    <message>
        <source>REALITY needs the server&apos;s public key (43 characters, as printed by xray x25519).</source>
        <translation>REALITY 需要服务器的公钥（43 个字符，如 xray x25519 所打印）。</translation>
    </message>
    <message>
        <source>Remove the trailing = padding.</source>
        <translation>移除尾部的 = 填充。</translation>
    </message>
    <message>
        <source>Paste the certificate as PEM text, starting with -----BEGIN CERTIFICATE-----.</source>
        <translation>以 PEM 文本形式粘贴证书，开头为 -----BEGIN CERTIFICATE-----。</translation>
    </message>
    <message>
        <source>Paste the private key as PEM text, starting with -----BEGIN PRIVATE KEY-----.</source>
        <translation>以 PEM 文本形式粘贴私钥，开头为 -----BEGIN PRIVATE KEY-----。</translation>
    </message>
    <message>
        <source>This is a hexadecimal SHA-256; write it in base64 (44 characters ending in =).</source>
        <translation>这是一个十六进制 SHA-256；以 base64 形式编写（44 个字符，以 = 结尾）。</translation>
    </message>
    <message>
        <source>Paste the ECH config list as base64, or as one -----BEGIN ECH CONFIGS----- block.</source>
        <translation>粘贴 ECH 配置列表为 base64，或作为一个 -----BEGIN ECH CONFIGS----- 块。</translation>
    </message>
    <message>
        <source>Reality cannot be combined with ECH. Turn off ECH in Advanced, or clear the Reality public key.</source>
        <translation>Reality 无法与 ECH 结合。在「高级」设置中关闭 ECH，或清除 Reality 公钥。</translation>
    </message>
    <message>
        <source>Reality cannot be used with Disable SNI. Turn off Disable SNI in Advanced.</source>
        <translation>Reality 无法配合“禁用 SNI”使用。在「高级」设置中关闭“禁用 SNI”。</translation>
    </message>
    <message>
        <source>Reality cannot be combined with TLS spoof. Set TLS Spoof to Off in Advanced.</source>
        <translation>Reality 无法与“TLS 欺骗”结合。在「高级」设置中设置“TLS 欺骗 (Spoof)”为关闭。</translation>
    </message>
    <message>
        <source>A custom certificate cannot be combined with certificate pins; keep only one of them.</source>
        <translation>自定义证书不能与证书固定一起使用；请只保留其中之一。</translation>
    </message>
    <message>
        <source>Set the client certificate and the client key together.</source>
        <translation>客户端证书和客户端钥匙一起设置。</translation>
    </message>
    <message>
        <source>TLS spoof needs an SNI. Turn off Disable SNI, or set TLS Spoof to Off.</source>
        <translation>TLS 伪装需要一个 SNI。关闭“禁用 SNI”，或者设置“TLS 伪装”为关闭。</translation>
    </message>
    <message>
        <source>TLS spoof needs a domain name as the SNI. Enter one in SNI, or set TLS Spoof to Off.</source>
        <translation>TLS 伪装需要一个作为 SNI 的域名。在 SNI 中输入一个，或设置“TLS 伪装”为关闭。</translation>
    </message>
    <message>
        <source>The spoof SNI must differ from the real SNI.</source>
        <translation>伪造的 SNI 必须与真实的 SNI 不同。</translation>
    </message>
    <message>
        <source>TLS spoof is skipped while TLS fragment or record fragment is on with a uTLS fingerprint.</source>
        <translation>当使用 uTLS 指纹是，若同时启用了 TLS 分片或记录分片时，TLS 伪装会被跳过。</translation>
    </message>
    <message>
        <source>sing-box cannot verify the REALITY ML-DSA-65 key, and ignoring it would weaken the server check. Only VLESS profiles running on Xray support it.</source>
        <translation>sing-box 无法验证 REALITY ML-DSA-65 钥匙，而且忽略它会削弱服务器检查。只有在 Xray 上运行的 VLESS 配置档支持它。</translation>
    </message>
    <message>
        <source>The REALITY short ID is invalid: it must be at most 16 hexadecimal characters, an even number of them.</source>
        <translation>REALITY 短 ID 无效：它必须至多 16 个十六进制字符，并且字符数必须为偶数。</translation>
    </message>
    <message>
        <source>The headers must alternate names and values separated by |, for example User-Agent|curl, so they were ignored.</source>
        <translation>请求头必须以用 | 间隔的名称和数值交替格式填写，例如 User-Agent|curl，因此它们被忽略了。</translation>
    </message>
    <message>
        <source>sing-box does not support the transport &quot;%1&quot;. Use ws, httpupgrade, http, grpc or quic.</source>
        <translation>sing-box 不支持传输 &quot;%1&quot;。请使用 ws、httpupgrade、http、grpc 或 quic。</translation>
    </message>
    <message>
        <source>The QUIC transport needs TLS. Set Security to tls.</source>
        <translation>QUIC 传输需要 TLS。设置安全性为 tls。</translation>
    </message>
    <message>
        <source>Reality does not work with the QUIC transport. Clear the Reality key, or choose another transport.</source>
        <translation>Reality 不能配合 QUIC 传输。请清除 Reality 钥匙，或选择其他传输方式。</translation>
    </message>
    <message>
        <source>uTLS is not used with the QUIC transport.</source>
        <translation>uTLS 不是与 QUIC 传输一起使用。</translation>
    </message>
    <message>
        <source>%1 The value was ignored.</source>
        <translation>%1 该值被忽略。</translation>
    </message>
    <message>
        <source>Expected text or a list of text, so the value was ignored.</source>
        <translation>预期为文本或文本列表，因此该值被忽略。</translation>
    </message>
    <message>
        <source>List items that are not text were ignored.</source>
        <translation>非文本的列表项被忽略。</translation>
    </message>
    <message>
        <source>Must not contain &quot;|&quot;, which separates headers in share links.</source>
        <translation>一定不能包含 “|”，它在分享链接中用于分隔标头。</translation>
    </message>
    <message>
        <source>Write Name=value pairs separated by spaces, quoting values that contain spaces, for example User-Agent=&quot;Mozilla/5.0&quot; X-Token=abc.</source>
        <translation>按“名称=值”的格式填写，多组用空格间隔，含空格的值用引号括住，例如 User-Agent=&quot;Mozilla/5.0&quot; X-Token=abc。</translation>
    </message>
    <message>
        <source>Enter only the domain name, without a port.</source>
        <translation>仅输入域名，不带端口。</translation>
    </message>
    <message>
        <source>The download leg must use XHTTP too: write &quot;network&quot;: &quot;xhttp&quot;.</source>
        <translation>下载方向也必须使用 XHTTP：填写 &quot;network&quot;: &quot;xhttp&quot;。</translation>
    </message>
    <message>
        <source>Add the download server&apos;s address, for example &quot;address&quot;: &quot;dl.example.com&quot;.</source>
        <translation>添加下载服务器的地址，例如 &quot;address&quot;: &quot;dl.example.com&quot;。</translation>
    </message>
    <message>
        <source>Must be a port number from 1 to 65535, written without quotes, for example &quot;port&quot;: 443.</source>
        <translation>必须是从 1 到 65535 之间的端口号，填写不带引号，例如 &quot;port&quot;: 443。</translation>
    </message>
    <message>
        <source>The extra value is not a valid JSON object, so it was ignored.</source>
        <translation>额外的值不是有效的 JSON 对象，因此被忽略。</translation>
    </message>
    <message>
        <source>Xray does not know the mKCP header type &quot;%1&quot;, so it was ignored.</source>
        <translation>Xray 无法识别 mKCP 头部类型“%1”，因此已忽略它。</translation>
    </message>
    <message>
        <source>Add the tunnel domain the server answers for, for example &quot;domains&quot;: [{&quot;names&quot;: [&quot;t.example.com&quot;]}].</source>
        <translation>添加服务器响应的隧道域名，例如 &quot;domains&quot;: [{&quot;names&quot;: [&quot;t.example.com&quot;]}] 。</translation>
    </message>
    <message>
        <source>Use udp://, tcp:// or https:// resolver addresses.</source>
        <translation>使用 udp://, tcp:// 或 https:// 解析器地址。</translation>
    </message>
    <message>
        <source>A DoH resolver needs &quot;tlsConfig&quot;, for example &quot;tlsConfig&quot;: {&quot;serverName&quot;: &quot;dns.google&quot;}.</source>
        <translation>一个 DoH 解析器需要 &quot;tlsConfig&quot;，例如 &quot;tlsConfig&quot;: {&quot;serverName&quot;: &quot;dns.google&quot;} 。</translation>
    </message>
    <message>
        <source>Over h3 the brutal congestion control crashes Xray; use bbr, reno or force-brutal.</source>
        <translation>基于 h3 时，brutal 拥塞控制会导致 Xray 崩溃；请改用 bbr、reno，或 force-brutal。</translation>
    </message>
    <message>
        <source>Set &quot;serverName&quot; in tlsConfig: over HTTP/2 Xray does not take it from the URL, so the TLS handshake fails.</source>
        <translation>请在 tlsConfig 中设置 &quot;serverName&quot;： 基于 HTTP/2 时，Xray 不会从该 URL 中读取它，因此 TLS 握手会失败。</translation>
    </message>
    <message>
        <source>Add a DNS resolver to send the tunnel&apos;s queries through, for example &quot;resolvers&quot;: [{&quot;addrs&quot;: [&quot;udp://8.8.8.8:53&quot;]}]; without one Xray cannot connect.</source>
        <translation>添加一个 DNS 解析器以通过它发送隧道的查询。例如&quot;resolvers&quot;: [{&quot;addrs&quot;: [&quot;udp://8.8.8.8:53&quot;]}] ；没有它的话，Xray 将无法建立连接。</translation>
    </message>
    <message>
        <source>The finalmask value is not valid JSON (%1), so it was ignored.</source>
        <translation>finalmask 值不是有效的 JSON (%1)，因此被忽略。</translation>
    </message>
    <message>
        <source>The finalmask value must be a JSON object, so it was ignored.</source>
        <translation>finalmask 值必须是一个 JSON 对象，因此被忽略。</translation>
    </message>
    <message>
        <source>XDRIVE has no share-link format. Import the server&apos;s JSON configuration instead.</source>
        <translation>XDRIVE 没有分享链接格式。请改为导入服务器的 JSON 配置。</translation>
    </message>
    <message>
        <source>The resolver IP list belongs to another installation, so it was not imported.</source>
        <translation>该解析器 IP 列表属于另一个安装，因此它未被导入。</translation>
    </message>
    <message>
        <source>Throne does not support the Xray transport &quot;%1&quot;. Use raw, xhttp, ws, httpupgrade, grpc, kcp or xdrive.</source>
        <translation>Throne 不支持 Xray 传输 &quot;%1&quot;。使用 raw, xhttp, ws, httpupgrade, grpc, kcp 或 xdrive 。</translation>
    </message>
    <message>
        <source>Xray does not support the security &quot;%1&quot;. Use none, tls or reality.</source>
        <translation>Xray 不支持安全性 &quot;%1&quot;。请使用 none、tls 或 reality。</translation>
    </message>
    <message>
        <source>REALITY works only with the raw, xhttp or grpc transport.</source>
        <translation>REALITY 仅适用于原生、xhttp 或 grpc 传输。</translation>
    </message>
    <message>
        <source>REALITY cannot use the hellogolang fingerprint; choose another one.</source>
        <translation>REALITY 不能使用 hellogolang 指纹；请选择另一个。</translation>
    </message>
    <message>
        <source>Xray does not know the fingerprint &quot;%1&quot;. Use chrome, firefox, safari, ios, android, edge, 360, qq, random, randomized or unsafe.</source>
        <translation>Xray 不知道指纹“%1”。请使用 chrome、firefox、safari、ios、android、edge、360、qq、random、randomized 或 unsafe。</translation>
    </message>
    <message>
        <source>Xray ignores the unknown cipher suite &quot;%1&quot;.</source>
        <translation>Xray 忽略未知的密码套件 &quot;%1&quot;。</translation>
    </message>
    <message>
        <source>Cipher suites only take effect with the unsafe fingerprint.</source>
        <translation>密码套件仅在使用 unsafe 指纹时生效。</translation>
    </message>
    <message>
        <source>Must be a SHA-256 in hexadecimal (64 characters, colons allowed).</source>
        <translation>必须是十六进制格式的 SHA-256 值（64 个字符；允许含冒号）。</translation>
    </message>
    <message>
        <source>Must be a base64 ECHConfigList, or domain+https://dns/dns-query to fetch it over DNS.</source>
        <translation>必须是 Base64 编码的 ECHConfigList，或 域名+https://dns/dns-query 以通过 DNS 获取它。</translation>
    </message>
    <message>
        <source>Fetching ECH over DNS needs a domain SNI; otherwise write it as domain+https://dns/dns-query.</source>
        <translation>通过 DNS 获取 ECH 需要域名 SNI；否则将其写为 “域名+https://dns/dns-query”。</translation>
    </message>
    <message>
        <source>Must start with /, for example /search?q=1.</source>
        <translation>必须以 / 开头，例如 /search?q=1。</translation>
    </message>
    <message>
        <source>Remove the Host header and put the host in the Host field instead.</source>
        <translation>删除 Host 头，并改为将主机放入 Host 字段中。</translation>
    </message>
    <message>
        <source>Both bounds must be greater than 0, for example 100-1000.</source>
        <translation>两个界限都必须大于0，例如 100-1000。</translation>
    </message>
    <message>
        <source>Cookie and header placement need the packet-up mode.</source>
        <translation>Cookie 和 header 放置需要 packet-up (数据包上行)模式。</translation>
    </message>
    <message>
        <source>GET uploads need the packet-up mode.</source>
        <translation>GET 上传需要使用 packet-up (数据包上行)模式。</translation>
    </message>
    <message>
        <source>Set a session ID length that starts above 0, for example 8-16.</source>
        <translation>设置一个大于0的会话 ID 长度，例如 8-16。</translation>
    </message>
    <message>
        <source>The table and length allow too few session IDs; use a longer length (for example 8-16) or a larger table.</source>
        <translation>表和长度允许的会话 ID 太少；使用更长的长度（例如 8-16）或更大的表。</translation>
    </message>
    <message>
        <source>The table must contain only ASCII characters.</source>
        <translation>该表必须仅包含 ASCII 字符。</translation>
    </message>
    <message>
        <source>Set either Max Connections or Max Concurrency, not both.</source>
        <translation>设置“最大连接数”或“最大并发数”中的一个，不要两者都设置。</translation>
    </message>
    <message>
        <source>Must be google-drive, s3, template or local.</source>
        <translation>必须是 google-drive, s3, template 或 local。</translation>
    </message>
    <message>
        <source>Enter the key prefix both ends share in the bucket.</source>
        <translation>输入两端在存储桶中共享的钥匙前缀。</translation>
    </message>
    <message>
        <source>The key prefix must be relative, without empty, &quot;.&quot; or &quot;..&quot; parts, backslashes or control characters.</source>
        <translation>钥匙前缀必须是相对的，不带空白、&quot;.&quot; 或 &quot;..&quot; 部分、反斜杠或控制字符。</translation>
    </message>
    <message>
        <source>Must be at least the MTU (%1).</source>
        <translation>必须至少为 MTU (%1)。</translation>
    </message>
    <message>
        <source>The xdns mask must be the last udp mask: it sends the DNS queries itself, so Xray skips the masks after it.</source>
        <translation>XDNS 掩码必须是最后的 UDP 掩码：它自行发送 DNS 查询，因此 Xray 跳过在其后的掩码。</translation>
    </message>
    <message>
        <source>Enter the directory both ends share.</source>
        <translation>输入通信双方共享的目录。</translation>
    </message>
    <message>
        <source>Enter the ID of the Google Drive folder both ends share.</source>
        <translation>请输入通信双方共享的 Google Drive 文件夹 ID。</translation>
    </message>
    <message>
        <source>S3 needs the access key ID and the secret access key; the session token is optional.</source>
        <translation>S3 需要访问钥匙 ID 和保密访问钥匙；会话令牌是可选的。</translation>
    </message>
    <message>
        <source>The S3 keys cannot contain control characters.</source>
        <translation>S3 钥匙不能包含控制字符。</translation>
    </message>
    <message>
        <source>The access key ID cannot contain spaces, commas or slashes.</source>
        <translation>访问钥匙 ID 不能包含空格、逗号或斜杠。</translation>
    </message>
    <message>
        <source>Google Drive needs the client ID, client secret and refresh token.</source>
        <translation>Google Drive 需要客户端 ID、客户端密钥和刷新令牌。</translation>
    </message>
    <message>
        <source>The template service needs a template describing the storage API.</source>
        <translation>模板 (template) 服务需要提供一个用于描述存储 API 的模板。</translation>
    </message>
    <message>
        <source>Enter the S3 endpoint URL, such as https://s3.example.com.</source>
        <translation>输入 S3 端点 URL，譬如 https://s3.example.com。</translation>
    </message>
    <message>
        <source>The endpoint must be an http or https URL without credentials, query or fragment.</source>
        <translation>端点必须是 http 或 https URL，且不含凭据、查询或片段。</translation>
    </message>
    <message>
        <source>The signing region cannot contain spaces, commas, slashes or control characters.</source>
        <translation>签名区域不能包含空格、逗号、斜杠或控制字符。</translation>
    </message>
    <message>
        <source>Enter the bucket both ends share.</source>
        <translation>输入两端共享的存储桶。</translation>
    </message>
    <message>
        <source>The bucket name cannot be &quot;.&quot; or &quot;..&quot;, nor contain slashes, backslashes or control characters.</source>
        <translation>存储桶名称不能是 &quot;.&quot; 或 &quot;.&quot;，也不能包含斜杠、反斜杠或控制字符。</translation>
    </message>
    <message>
        <source>Virtual-hosted addressing needs a DNS-compatible bucket name: lowercase letters, digits, dots and hyphens.</source>
        <translation>虚拟主机式寻址需要一个兼容 DNS 的存储桶名称：仅含小写字母、数字、点和连字符。</translation>
    </message>
    <message>
        <source>Virtual-hosted addressing needs a DNS endpoint, not an IP address. Turn on path-style addressing instead.</source>
        <translation>虚拟主机式寻址需要 DNS 端点，不能使用 IP 地址。请改为开启路径式寻址。</translation>
    </message>
    <message>
        <source>The front port is used only together with a front address.</source>
        <translation>前置端口仅与前置地址一起使用。</translation>
    </message>
    <message>
        <source>The DNS address is empty.</source>
        <translation>DNS 地址是空的。</translation>
    </message>
    <message>
        <source>Name an interface after dhcp://, or use dhcp://auto.</source>
        <translation>请在 dhcp:// 后指定接口名称，或使用 dhcp://auto。</translation>
    </message>
    <message>
        <source>&quot;%1&quot; is not a valid interface name.</source>
        <translation>&quot;%1&quot; 不是有效的接口名称。</translation>
    </message>
    <message>
        <source>&quot;%1&quot; is not a supported DNS scheme; use udp, tcp, tls, quic, https, h3, dhcp or local.</source>
        <translation>&quot;%1&quot; 不是受支持的 DNS 方案；请使用 udp、tcp、tls、quic、https、h3、dhcp 或 local。</translation>
    </message>
    <message>
        <source>Only https:// and h3:// DNS addresses take a path.</source>
        <translation>只有 https:// 和 h3:// 的 DNS 地址可以有路径。</translation>
    </message>
    <message>
        <source>The IPv6 address is missing its closing bracket.</source>
        <translation>IPv6 地址缺少其闭合括号。</translation>
    </message>
    <message>
        <source>Only a port may follow a bracketed IPv6 address.</source>
        <translation>只有端口可以跟随括号内的 IPv6 地址。</translation>
    </message>
    <message>
        <source>&quot;%1&quot; is not a valid IPv6 address.</source>
        <translation>“%1” 不是有效的 IPv6 地址。</translation>
    </message>
    <message>
        <source>&quot;%1&quot; is not a valid IPv6 address; put it in brackets to add a port.</source>
        <translation>“%1” 不是有效的 IPv6 地址；请将其放在方括号中以添加端口。</translation>
    </message>
    <message>
        <source>The DNS server address is missing.</source>
        <translation>DNS 服务器地址缺失。</translation>
    </message>
    <message>
        <source>&quot;%1&quot; is not a valid IPv4 address.</source>
        <translation>“%1” 不是有效的 IPv4 地址。</translation>
    </message>
    <message>
        <source>&quot;%1&quot; is not a valid IP address or host name.</source>
        <translation>“%1” 不是有效的 IP 地址或主机名。</translation>
    </message>
    <message>
        <source>The port is missing after the colon.</source>
        <translation>在冒号后缺少端口。</translation>
    </message>
    <message>
        <source>&quot;%1&quot; is not a valid port; use a number from 1 to 65535.</source>
        <translation>“%1” 不是有效的端口；请使用从 1 到 65535 之间的数。</translation>
    </message>
    <message>
        <source>Use two-letter country codes separated by commas, for example DE,NL,FR; put ! before a code to exclude that country, for example !RU.</source>
        <translation>使用以逗号间隔的两字母的国家/地区代码，例如 DE,NL,FR；要排除某个国家/地区，请在代码前加 !，例如 !RU。</translation>
    </message>
    <message>
        <source>Cannot run more profiles than are ranked (%1).</source>
        <translation>无法运行比被排名数量 (%1) 更多的配置档。</translation>
    </message>
    <message>
        <source>The full sweep cannot run more often than the check interval (%1 s).</source>
        <translation>完整扫描的运行频率不能高于检查间隔（%1 秒）。</translation>
    </message>
    <message>
        <source>Watching the profile in use cannot be slower than the check interval (%1 s).</source>
        <translation>正在使用的配置文件的监视速度不能慢于检查间隔（%1 秒）。</translation>
    </message>
    <message>
        <source>Check closely at least as many profiles as are kept ready (%1).</source>
        <translation>至少仔细检查与准备就绪数量 (%1) 相同的配置档。</translation>
    </message>
    <message>
        <source>Cannot check closely more profiles than are run (%1).</source>
        <translation>无法仔细检查比运行数量 (%1) 更多的配置档。</translation>
    </message>
    <message>
        <source>Removed the Tor executable path from the imported configuration; add it back only if you trust that program.</source>
        <translation>已从导入的配置中移除 Tor 可执行文件路径；仅在你信任该程序时才将添加回去。</translation>
    </message>
    <message>
        <source>Removed the Tor command-line arguments from the imported configuration: they can make Tor start other programs.</source>
        <translation>已从导入的配置中移除 Tor 命令行参数：它们可能会使 Tor 启动其他程序。</translation>
    </message>
    <message>
        <source>Removed the Tor data directory from the imported configuration: Tor reads its torrc file from there, which can make it start other programs.</source>
        <translation>已从导入的配置中删除 Tor 数据目录：Tor 会从那里读取其 torrc 文件，这可能会导致它启动其他程序。</translation>
    </message>
    <message>
        <source>Removed a Tor option with an invalid name from the imported configuration: option names use only letters, digits and underscores, like UseBridges.</source>
        <translation>从导入的配置中删除了一个名称无效的 Tor 选项：选项名称仅使用字母、数字和下划线，如 UseBridges。</translation>
    </message>
    <message>
        <source>Removed the Tor option %1 from the imported configuration: it makes Tor start a program on this computer.</source>
        <translation>已从导入的配置中移除 Tor 选项 %1：它会使 Tor 启动这台计算机上的程序。</translation>
    </message>
    <message>
        <source>Removed the %1 wrapper path from the imported configuration; add it back only if you trust that program.</source>
        <translation>已从导入的配置中移除 %1 封装器路径；仅在你信任该程序时才添加回去。</translation>
    </message>
    <message>
        <source>Removed the Tailscale SSH server from the imported configuration: it would let devices on that tailnet open a shell on this computer.</source>
        <translation>已从导入的配置中移除 Tailscale SSH 服务器：它会允许在该 Tailnet 上的设备在这台计算机上打开 shell。</translation>
    </message>
    <message>
        <source>Removed the Taildrop folder from the imported configuration: devices on that tailnet could place files there. Received files now go to the default folder.</source>
        <translation>已从导入的配置中移除 Taildrop 文件夹：该 tailnet 上的设备曾可以放置文件在那里。现在接收的文件将放入默认文件夹。</translation>
    </message>
    <message>
        <source>Removed from the imported configuration: it would open a port on this computer that other devices could reach.</source>
        <translation>已从导入的配置中移除：它会在这台计算机上打开一个端口，其他设备可以访问该端口。</translation>
    </message>
    <message>
        <source>Removed from the imported configuration: it would write a file on this computer at a path the sender chose.</source>
        <translation>已从导入的配置中移除：它会在这台计算机上写入文件于发送方选择的路径。</translation>
    </message>
    <message>
        <source>Removed the reverse proxy from the imported configuration: it would let the server reach this computer and its network.</source>
        <translation>已从导入的配置中移除反向代理：它会让服务器访问这台计算机及其网络。</translation>
    </message>
    <message>
        <source>Removed this inbound from the imported configuration: it could let other devices connect through this computer. Only tun inbounds and local proxies that listen on 127.0.0.1 or ::1 are kept.</source>
        <translation>已从导入的配置中移除這個入站：它可能允许其他设备通过这个计算机连接。仅保留在 127.0.0.1 或 ::1 上监听的 tun 入站和本地代理。</translation>
    </message>
    <message>
        <source>Removed the Clash API from the imported configuration: web pages and programs on this computer could use it to control the core, and its external_ui setting downloads files.</source>
        <translation>已从导入的配置中移除 Clash API：这台计算机上的网页和程序可能会使用它来控制核心，以及其 external_ui 设置下载文件。</translation>
    </message>
    <message>
        <source>Removed the V2Ray API from the imported configuration: Throne&apos;s core is built without it and would not start, and it would open a statistics port on this computer.</source>
        <translation>已从导入的配置中移除 V2Ray API：Throne 的核心在没有它的情况下构建，无法启动，并且它会在这台计算机上打开一个统计端口。</translation>
    </message>
    <message>
        <source>Removed the inbounds from the imported configuration: Throne adds its own local inbound, and these could let other devices connect through this computer.</source>
        <translation>已从导入的配置中删除入站：Throne 会添加它自己的本地入站，而这些入站可能会让其他设备通过这台计算机进行连接。</translation>
    </message>
    <message>
        <source>Removed the environment variables from the imported configuration: they would apply to Throne&apos;s core and to the programs it starts.</source>
        <translation>已从导入的配置中移除环境变量：它们会应用到 Throne 的核心以及它启动的程序。</translation>
    </message>
    <message>
        <source>Removed the geodata updater from the imported configuration: it would download files from addresses the sender chose and replace files in Throne&apos;s folder.</source>
        <translation>已从导入的配置中移除地理数据更新器：它会从发送者选择的地址下载文件并替换 Throne 的文件夹中的文件。</translation>
    </message>
    <message>
        <source>Removed the webhook from this routing rule: it would send the details of every matching connection to an address the sender chose.</source>
        <translation>已从这个路由规则中移除 webhook：它会发送每个匹配连接的详细信息到发送者选择的地址。</translation>
    </message>
    <message>
        <source>Comments are not allowed in the JSON.</source>
        <translation>JSON 中不允许有注释。</translation>
    </message>
    <message>
        <source>Add the outbound type, for example &quot;type&quot;: &quot;socks&quot;.</source>
        <translation>添加出站类型，例如 &quot;type&quot;: &quot;socks&quot;。</translation>
    </message>
    <message>
        <source>Add the protocol, for example &quot;protocol&quot;: &quot;vless&quot;.</source>
        <translation>添加协议，例如 &quot;protocol&quot;: &quot;vless&quot;。</translation>
    </message>
    <message>
        <source>&quot;outbounds&quot; must be a list, for example &quot;outbounds&quot;: [].</source>
        <translation>&quot;outbounds&quot; (出站)必须是一个列表，例如 &quot;outbounds&quot;: [].</translation>
    </message>
    <message>
        <source>Must not contain spaces.</source>
        <translation>不得包含空格。</translation>
    </message>
    <message>
        <source>Enter a user name too; without one the password is not sent.</source>
        <translation>也输入用户名；没有用户名，密码将不发送。</translation>
    </message>
    <message>
        <source>The headers must alternate names and values separated by commas, for example User-Agent,curl, so they were ignored.</source>
        <translation>标头必须是以逗号间隔的交替的名称和值，例如 User-Agent,curl，所以它们被忽略了。</translation>
    </message>
    <message>
        <source>Port hopping needs at least 5s between hops, for example 30s.</source>
        <translation>端口跳变需要跳变间隔至少 5 秒，例如 30 秒。</translation>
    </message>
    <message>
        <source>Needs a Hop Interval above 0 as well, for example 30s with a maximum of 60s.</source>
        <translation>还需要一个大于 0 的跳变间隔，例如 30 秒，最大为 60 秒。</translation>
    </message>
    <message>
        <source>Must not be shorter than Hop Interval.</source>
        <translation>不得短于“跳变间隔”。</translation>
    </message>
    <message>
        <source>Ignored without a Hop Interval; set one as well, for example 30s with a maximum of 60s.</source>
        <translation>在没有“跳变间隔”的情况下被忽略；也设置一个，例如 30 秒，最大为 60 秒。</translation>
    </message>
    <message>
        <source>Hysteria v1 needs both Up Mbps and Down Mbps; %1 up and %2 down is used for a missing one. Set your real line speed for the best results.</source>
        <translation>Hysteria v1 需要同时设置上行和下行 Mbps；缺少任一值时，将使用上行 %1 Mbps、下行 %2 Mbps。为获得最佳效果，请设置实际线路速度。</translation>
    </message>
    <message>
        <source>Packet sizes must be from 0 to 2048; 0 uses the default.</source>
        <translation>数据包大小必须是 0 到 2048 之间；0 使用默认值。</translation>
    </message>
    <message>
        <source>The minimum packet size must not exceed the maximum; 0 means the default, 512 for the minimum and 1200 for the maximum.</source>
        <translation>最小数据包大小不得超过最大的；0 表示默认值，最小为 512，最大为 1200。</translation>
    </message>
    <message>
        <source>Not valid base64. Choose STRING as the authentication type for a plain-text payload.</source>
        <translation>不是有效的 base64。对于纯文本负载，请选择 STRING 作为身份验证类型。</translation>
    </message>
    <message>
        <source>Choose an authentication type; without one the payload is not sent.</source>
        <translation>选择一种身份验证类型；若无身份验证则载荷不会被发送。</translation>
    </message>
    <message>
        <source>Realm needs a URL, an ID and at least one STUN server.</source>
        <translation>Realm 需要一个 URL、一个 ID 和至少一个 STUN 服务器。</translation>
    </message>
    <message>
        <source>Realm URL: %1</source>
        <translation>Realm 网址: %1</translation>
    </message>
    <message>
        <source>STUN Servers: %1</source>
        <translation>STUN 服务器: %1</translation>
    </message>
    <message>
        <source>Mapping Timeout: %1</source>
        <translation>映射超时: %1</translation>
    </message>
    <message>
        <source>Mapping Lifetime: %1</source>
        <translation>映射寿命: %1</translation>
    </message>
    <message>
        <source>Not a bandwidth such as 100 Mbps or 50 MB/s, so it was ignored.</source>
        <translation>不是如 100Mbps 或 50MB/s 这样的带宽，因此被忽略了。</translation>
    </message>
    <message>
        <source>Only the udp protocol is supported, so this server cannot be reached.</source>
        <translation>只支持 UDP 协议，因此无法访问这个服务器。</translation>
    </message>
    <message>
        <source>The certificate pin must be a SHA-256 hash in hex (64 characters) or base64 (44 characters).</source>
        <translation>证书固定值必须是十六进制（64 个字符）或 base64（44 个字符）的 SHA-256 哈希。</translation>
    </message>
    <message>
        <source>Not a speed such as 100 Mbps or 12 MBps, so it was ignored.</source>
        <translation>不是类似 100Mbps 或 12MBps 这样的速度，所以被忽略了。</translation>
    </message>
    <message>
        <source>Item %1 (&quot;%2&quot;): %3</source>
        <translation>项目 %1 (&quot;%2&quot;): %3</translation>
    </message>
    <message>
        <source>Must be the base64 key from WARP registration (standard base64 with = padding).</source>
        <translation>必须是来自 WARP 注册的 base64 钥匙（带 = 填充的标准 base64）。</translation>
    </message>
    <message>
        <source>At least one local address is required, for example 10.0.0.2/32.</source>
        <translation>至少需要一个本地地址，例如 10.0.0.2/32。</translation>
    </message>
    <message>
        <source>Ports must be from 1 to 65535.</source>
        <translation>端口必须在 1 到 65535 之间。</translation>
    </message>
    <message>
        <source>The range starts after it ends; write it as %1-%2.</source>
        <translation>范围在它结束后开始；写为 %1-%2。</translation>
    </message>
    <message>
        <source>The = padding at the end is missing; copy the whole pattern.</source>
        <translation>在末尾缺少 = 填充；请复制整个模式。</translation>
    </message>
    <message>
        <source>Write each header as Name=&quot;value&quot;, for example X-Username=&quot;user&quot; X-Password=&quot;pass&quot;.</source>
        <translation>每个请求头写成`名称=&quot;值&quot;`形式，例如 X-Username=&quot;user&quot; X-Password=&quot;pass&quot;。</translation>
    </message>
    <message>
        <source>Header name &quot;%1&quot;: %2</source>
        <translation>请求头名称 &quot;%1&quot;: %2</translation>
    </message>
    <message>
        <source>A header line without a name and a colon was ignored; write each line as Name: value.</source>
        <translation>没有名称和冒号的请求头行被忽略；每行写成“名称: 值”。</translation>
    </message>
    <message>
        <source>Remove the part from ? or #; the server takes only a path such as /vpn.</source>
        <translation>删除从 ? 或 # 开始的部分；服务器只接受像 /vpn 这样的路径。</translation>
    </message>
    <message>
        <source>A % must start an escape such as %20.</source>
        <translation>一个 % 必须开始一个转义（即%后必须跟有效转义序列），例如 %20。</translation>
    </message>
    <message>
        <source>The core raises values below 576 to 576.</source>
        <translation>核心提高低于 576 的值到 576。</translation>
    </message>
    <message>
        <source>Write the fingerprint without colons between the bytes, or as sha1:, sha256: or pin-sha256:.</source>
        <translation>在字节间编写指纹不带冒号，或者写为 sha1:, sha256: 或 pin-sha256: 形式。</translation>
    </message>
    <message>
        <source>After pin-sha256: write the base64 SHA-256 of the public key (44 characters).</source>
        <translation>在 pin-sha256: 后写入公钥的 base64 编码 SHA-256 值（44 个字符）。</translation>
    </message>
    <message>
        <source>Use at least 4 hexadecimal characters of the fingerprint (up to %1).</source>
        <translation>使用至少4个十六进制字符的指纹（最多 %1 个字符）。</translation>
    </message>
    <message>
        <source>The core raises values below 2s to 2s.</source>
        <translation>核心将低于2秒的值提高至2秒。</translation>
    </message>
    <message>
        <source>Add the key that belongs to this certificate.</source>
        <translation>添加属于这个证书的钥匙。</translation>
    </message>
    <message>
        <source>Add the certificate that belongs to this key.</source>
        <translation>添加属于这个钥匙的证书。</translation>
    </message>
    <message>
        <source>Needs a submission key, or a form ID and a field name.</source>
        <translation>需要一个 submission key (提交键)，或表单 ID 和字段名。</translation>
    </message>
    <message>
        <source>A promoted field is asked for when connecting, so it cannot also have a value.</source>
        <translation>连接时会要求提供一个提升字段，因此它不能也有一个值。</translation>
    </message>
    <message>
        <source>Set the token secret either directly or as a file, not both.</source>
        <translation>令牌密钥，直接设置或者设为文件，不要两种都用。</translation>
    </message>
    <message>
        <source>The software token needs its secret, directly or as a file.</source>
        <translation>软件令牌需要其密钥，直接提供或以文件形式提供。</translation>
    </message>
    <message>
        <source>Without a token mode these settings are not used or saved; pick a mode or clear them.</source>
        <translation>没有令牌模式，这些设置不会被使用或保存；请选择一种模式或清除它们。</translation>
    </message>
    <message>
        <source>Only hotp tokens use the counter.</source>
        <translation>只有 hotp 令牌使用计数器。</translation>
    </message>
    <message>
        <source>Fill in all three mobile fields or leave them all empty.</source>
        <translation>填写所有三个移动字段，或全部留空。</translation>
    </message>
    <message>
        <source>Compression is disabled, so the mode must be empty or stateless.</source>
        <translation>压缩被禁用，因此模式必须为空或 stateless (无状态)。</translation>
    </message>
    <message>
        <source>Removed the link to a one-time password entry; choose the entry again in the profile&apos;s settings.</source>
        <translation>已移除指向一次性密码条目的链接；在配置档的设置中再次选择该条目。</translation>
    </message>
    <message>
        <source>Removed the CSD wrapper path from the imported profile; set it again in Advanced settings if you trust it.</source>
        <translation>已从导入的配置档中移除 CSD 封装器路径；如果您信任它，请在「高级」设置中再次设置它。</translation>
    </message>
    <message>
        <source>Removed the HIP wrapper path from the imported profile; set it again in Advanced settings if you trust it.</source>
        <translation>已从导入的配置档中移除 HIP 封装器路径；如果您信任它，请在「高级」设置中再次设置它。</translation>
    </message>
    <message>
        <source>Removed the TNCC wrapper path from the imported profile; set it again in Advanced settings if you trust it.</source>
        <translation>已从导入的配置档中移除 TNCC 封装器路径；如果您信任它，请在「高级」设置中再次设置它。</translation>
    </message>
    <message>
        <source>Must be 0 or at least 1s.</source>
        <translation>必须为 0 或至少1秒。</translation>
    </message>
    <message>
        <source>Separate the ciphers with commas, not colons.</source>
        <translation>用逗号（而不是冒号）间隔密码。</translation>
    </message>
    <message>
        <source>Not an OpenVPN cipher name. Use a name such as AES-256-GCM, CHACHA20-POLY1305 or AES-256-CBC.</source>
        <translation>不是 OpenVPN 密码名称。请使用诸如 AES-256-GCM、CHACHA20-POLY1305 或 AES-256-CBC 这样的名称。</translation>
    </message>
    <message>
        <source>Use the certificate&apos;s SHA-256 fingerprint: 64 hexadecimal characters, colons allowed.</source>
        <translation>使用证书的 SHA-256 指纹: 64 个十六进制字符，允许用冒号。</translation>
    </message>
    <message>
        <source>Use a hexadecimal key usage value such as a0 or 88.</source>
        <translation>使用十六进制密钥使用值，例如 a0 或 88。</translation>
    </message>
    <message>
        <source>Use a name such as TLS Web Server Authentication or an OID such as 1.3.6.1.5.5.7.3.1.</source>
        <translation>使用如 TLS Web 服务器身份验证之类的名称或诸如 1.3.6.1.5.5.7.3.1 之类的 OID。</translation>
    </message>
    <message>
        <source>Remove the empty entry between two colons.</source>
        <translation>删除两个冒号之间的空条目。</translation>
    </message>
    <message>
        <source>&quot;%1&quot; is not a TLS 1.2 cipher suite the core supports, for example ECDHE-RSA-AES256-GCM-SHA384.</source>
        <translation>&quot;%1&quot; 不是核心支持的 TLS 1.2 密码套件，例如 ECDHE-RSA-AES256-GCM-SHA384。</translation>
    </message>
    <message>
        <source>&quot;%1&quot; is not a TLS group the core supports. Use X25519, P-256, P-384 or P-521, separated by colons.</source>
        <translation>&quot;%1&quot; 不是核心支持的 TLS 组。请使用 X25519、P-256、P-384 或 P-521，用冒号间隔。</translation>
    </message>
    <message>
        <source>Must be 0 (off) or at least 68.</source>
        <translation>必须为 0（关闭）或至少 68。</translation>
    </message>
    <message>
        <source>Write the IPv6 address without brackets.</source>
        <translation>写入不带括号的 IPv6 地址。</translation>
    </message>
    <message>
        <source>Static key mode does not use this setting; clear it or switch the mode to TLS.</source>
        <translation>静态钥匙模式不使用这个设置；清空它或切换模式到 TLS。</translation>
    </message>
    <message>
        <source>Only static key mode uses Cipher; in TLS mode list the cipher under Data Ciphers.</source>
        <translation>只有静态钥匙模式使用密码；在 TLS 模式下，在「数据密码」下列出密码。</translation>
    </message>
    <message>
        <source>Key direction applies only to static key mode; for tls_auth set the control channel wrap direction.</source>
        <translation>钥匙方向仅适用于静态钥匙模式；对于 tls_auth，请设置控制通道封装方向。</translation>
    </message>
    <message>
        <source>Only static key mode uses a static key; clear it or switch the mode to static_key.</source>
        <translation>只有静态钥匙模式使用静态钥匙；清除它或切换模式到 static_key。</translation>
    </message>
    <message>
        <source>Static key mode needs the static key, pasted or as a file.</source>
        <translation>静态钥匙模式需要静态钥匙，可粘贴或作为文件。</translation>
    </message>
    <message>
        <source>Static key mode needs a cipher, for example AES-256-CBC.</source>
        <translation>静态钥匙模式需要一个密码算法，例如 AES-256-CBC。</translation>
    </message>
    <message>
        <source>Static key mode supports only the CBC ciphers, such as AES-256-CBC, and NONE.</source>
        <translation>静态钥匙模式仅支持 CBC 密码，如 AES-256-CBC 和 NONE。</translation>
    </message>
    <message>
        <source>Static key mode needs the tunnel address, for example 10.8.0.2/24.</source>
        <translation>静态钥匙模式需要隧道地址，例如 10.8.0.2/24。</translation>
    </message>
    <message>
        <source>Add the IPv4 peer address for the IPv4 tunnel address.</source>
        <translation>添加 IPv4 隧道地址的 IPv4 对等地址。</translation>
    </message>
    <message>
        <source>Add the IPv6 peer address for the IPv6 tunnel address.</source>
        <translation>添加 IPv6 隧道地址的 IPv6 对等地址。</translation>
    </message>
    <message>
        <source>The IPv4 peer address needs an IPv4 tunnel address.</source>
        <translation>IPv4 对等地址需要一个 IPv4 隧道地址。</translation>
    </message>
    <message>
        <source>The IPv6 peer address needs an IPv6 tunnel address.</source>
        <translation>IPv6 对等地址需要一个 IPv6 隧道地址。</translation>
    </message>
    <message>
        <source>TLS mode needs the CA certificate or a peer fingerprint to verify the server.</source>
        <translation>TLS 模式需要 CA 证书或者对端指纹以验证服务器。</translation>
    </message>
    <message>
        <source>Add the client key that belongs to this certificate.</source>
        <translation>添加属于这个证书的客户端钥匙。</translation>
    </message>
    <message>
        <source>Add the client certificate that belongs to this key.</source>
        <translation>添加属于这个钥匙的客户端证书。</translation>
    </message>
    <message>
        <source>Set both pasted and as a file; keep only one of them.</source>
        <translation>粘贴内容和按文件形式都设置了；请仅保留其中一种。</translation>
    </message>
    <message>
        <source>Static key mode does not use the control channel wrap; clear its key.</source>
        <translation>静态钥匙模式不使用控制通道封装；清除其钥匙。</translation>
    </message>
    <message>
        <source>Pick the control channel wrap type for this key: tls_auth, tls_crypt or tls_crypt_v2.</source>
        <translation>为这个钥匙选取控制通道封装类型: tls_auth、tls_crypt 或 tls_crypt_v2。</translation>
    </message>
    <message>
        <source>The control channel wrap needs its key, pasted or as a file.</source>
        <translation>控制通道封装需要其密钥，可以粘贴或作为文件。</translation>
    </message>
    <message>
        <source>The key direction applies only to tls_auth.</source>
        <translation>钥匙方向仅适用于 tls_auth。</translation>
    </message>
    <message>
        <source>Use either the remote certificate EKU or the remote certificate TLS role, not both.</source>
        <translation>使用远程证书 EKU 或远程证书 TLS 角色其中一个，不能两者都用。</translation>
    </message>
    <message>
        <source>The maximum TLS version is below the minimum, which is 1.2 unless set.</source>
        <translation>TLS 最高版本低于最低版本；最低版本为 1.2，除非另行设置。</translation>
    </message>
    <message>
        <source>MSS clamping is disabled; clear the MSS fix value or enable it.</source>
        <translation>MSS 钳制功能被禁用；请清除 MSS 修正值，或启用 MSS 钳制功能。</translation>
    </message>
    <message>
        <source>MSS clamping is disabled; clear the MSS fix mode or enable it.</source>
        <translation>MSS 钳制功能被禁用；请清除 MSS 修正模式，或启用 MSS 钳制功能。</translation>
    </message>
    <message>
        <source>The MSS fix mode needs an MSS fix value.</source>
        <translation>MSS 修正模式需要一个 MSS 修正值。</translation>
    </message>
    <message>
        <source>Ping restart is disabled; clear the restart time or enable it.</source>
        <translation>Ping 重启被禁用；请清除重启时间或启用该功能。</translation>
    </message>
    <message>
        <source>Renegotiation is disabled; clear the interval or enable it.</source>
        <translation>重新协商被禁用；请清除时间间隔或启用该功能。</translation>
    </message>
    <message>
        <source>%1 is replaced by a one-time code only when an authenticator entry is selected.</source>
        <translation>仅当选择了身份验证器条目时，%1 才会被一次性代码替换。</translation>
    </message>
    <message>
        <source>A backslash at the end has nothing to escape.</source>
        <translation>在末尾处的反斜杠没有转义的内容。</translation>
    </message>
    <message>
        <source>Every option needs a name, as in obfs=http;obfs-host=example.com.</source>
        <translation>每个选项都需要一个名称，例如 obfs=http;obfs-host=example.com 中。</translation>
    </message>
    <message>
        <source>2022-blake3-chacha20-poly1305 takes a single key; several keys joined with : need an AES 2022 method.</source>
        <translation>2022-blake3-chacha20-poly1305 采用单个钥匙；用 : 串联的多个钥匙需要 AES 2022 方法。</translation>
    </message>
    <message>
        <source>Item %1: %2</source>
        <translation>项目 %1: %2</translation>
    </message>
    <message>
        <source>Unknown obfs mode &quot;%1&quot;. Use obfs=http or obfs=tls.</source>
        <translation>未知 obfs 模式 &quot;%1&quot;。使用 obfs=http 或 obfs=tls。</translation>
    </message>
    <message>
        <source>Unknown mode &quot;%1&quot;. Use mode=websocket or mode=quic.</source>
        <translation>未知模式 &quot;%1&quot;。使用 mode=websocket 或 mode=quic。</translation>
    </message>
    <message>
        <source>mux must be a whole number, for example mux=1.</source>
        <translation>mux 必须是一个整数，例如 mux=1。</translation>
    </message>
    <message>
        <source>The link is neither ss://method:password@host:port nor valid base64.</source>
        <translation>该链接既不是“ss://方法:密码@主机:端口”格式，也不是有效的 base64。</translation>
    </message>
    <message>
        <source>The method and password are not valid base64.</source>
        <translation>方法和密码不是有效的 base64。</translation>
    </message>
    <message>
        <source>The decoded userinfo must be method:password, for example aes-256-gcm:secret.</source>
        <translation>解码后的 userinfo (用户信息)必须是“方法:密码”，例如 aes-256-gcm:secret。</translation>
    </message>
    <message>
        <source>Unsupported plugin; only obfs and v2ray-plugin work.</source>
        <translation>不支持的插件；只有 obfs 和 v2ray-plugin 可用。</translation>
    </message>
    <message>
        <source>The version must be 1, 2 or 3.</source>
        <translation>版本必须是 1、2 或 3。</translation>
    </message>
    <message>
        <source>ShadowTLS v2 and v3 need the password set on the server.</source>
        <translation>ShadowTLS v2 和 v3 需要在服务器上设置的密码。</translation>
    </message>
    <message>
        <source>Only Snell versions 4 and 6 are supported.</source>
        <translation>仅支持 Snell 版本 4 和 6。</translation>
    </message>
    <message>
        <source>Unsupported obfuscation; Snell can use only none, http or tls.</source>
        <translation>不支持的混淆；Snell 只能使用 none、http 或 tls。</translation>
    </message>
    <message>
        <source>Unknown network, so both TCP and UDP are used. Use tcp or udp.</source>
        <translation>未知网络（传输协议），因此 TCP 和 UDP 都被使用。使用 tcp 或 udp。</translation>
    </message>
    <message>
        <source>Must be at most 255 bytes; this one is %1 bytes.</source>
        <translation>必须至多 255 字节；这个是 %1 字节。</translation>
    </message>
    <message>
        <source>Snell v6 has no obfuscation, so these settings are ignored.</source>
        <translation>Snell v6 没有混淆功能，因此这些设置会被忽略。</translation>
    </message>
    <message>
        <source>The obfuscation host is used only with http or tls obfuscation, so it is ignored.</source>
        <translation>混淆主机仅配合 HTTP 或 TLS 混淆使用，因此会被忽略。</translation>
    </message>
    <message>
        <source>The mode is used only by Snell v6, so it is ignored.</source>
        <translation>该模式仅被 Snell v6 使用，因此被忽略。</translation>
    </message>
    <message>
        <source>Unknown SOCKS version, so SOCKS5 was used. Use 4, 4a or 5.</source>
        <translation>未知 SOCKS 版本，因此使用了 SOCKS5。请使用 4、4a 或 5。</translation>
    </message>
    <message>
        <source>SOCKS4 sends no password, so it is ignored.</source>
        <translation>SOCKS4 不发送密码，因此被忽略。</translation>
    </message>
    <message>
        <source>An entry that is not valid base64 was ignored.</source>
        <translation>不是有效 base64 的条目已被忽略。</translation>
    </message>
    <message>
        <source>This is a public key; paste the private key, the block that starts with -----BEGIN and ends with PRIVATE KEY-----.</source>
        <translation>这是一个公钥；粘贴私钥，即以 -----BEGIN 开始并以 PRIVATE KEY----- 结束的区块。</translation>
    </message>
    <message>
        <source>PuTTY keys are not supported; export the key in OpenSSH format with PuTTYgen.</source>
        <translation>PuTTY 钥匙不被支持；请用 PuTTYgen 以 OpenSSH 格式导出钥匙。</translation>
    </message>
    <message>
        <source>Not a private key: paste the whole block from -----BEGIN to the PRIVATE KEY----- line.</source>
        <translation>不是私钥：请粘贴从 -----BEGIN 到 PRIVATE KEY----- 行的整个区块。</translation>
    </message>
    <message>
        <source>Use the server&apos;s public key line, for example ssh-ed25519 AAAAC3Nza... as printed by ssh-keyscan.</source>
        <translation>使用服务器的公钥行，例如 “ssh-ed25519AAAAC3Nza...”，如 ssh-keyscan 打印所示。</translation>
    </message>
    <message>
        <source>Servers expect a version starting with SSH-2.0-, for example SSH-2.0-OpenSSH_9.6.</source>
        <translation>服务器期望版本以 SSH-2.0- 开头，例如 SSH-2.0-OpenSSH_9.6。</translation>
    </message>
    <message>
        <source>Without a password or a private key, only a server that allows logging in without authentication accepts this profile.</source>
        <translation>没有密码或私钥时，只有允许无需身份验证即可登录的服务器才会接受这个配置档。</translation>
    </message>
    <message>
        <source>Both a pasted key and a key file are set; the pasted key is used.</source>
        <translation>同时设置了粘贴的钥匙和钥匙文件；使用粘贴的钥匙。</translation>
    </message>
    <message>
        <source>Do not advertise the default route; tick Advertise exit node instead.</source>
        <translation>不要通告默认路由；请改为勾选“通告出口节点”。</translation>
    </message>
    <message>
        <source>A node that advertises itself as an exit node cannot use another one; clear the exit node or untick Advertise exit node.</source>
        <translation>通告自己为出口节点的节点不能使用另一个出口节点；清除该出口节点或取消勾选“通告出口节点”。</translation>
    </message>
    <message>
        <source>The link sets neither security nor sni, so the profile connects without TLS, which most Trojan servers refuse.</source>
        <translation>该链接未设置安全类型或 SNI，因此配置档将不用 TLS 连接；大多数 Trojan 服务器拒绝此类连接。</translation>
    </message>
    <message>
        <source>Use 1 to 32 bytes in hex, optionally followed by /mask of the same length, for example a0b0/f0f0.</source>
        <translation>使用 1 到 32 字节的十六进制，可选后跟同长度的“/掩码”，例如 a0b0/f0f0。</translation>
    </message>
    <message>
        <source>TrustTunnel needs both a username and a password.</source>
        <translation>TrustTunnel (信任隧道)需要用户名和密码。</translation>
    </message>
    <message>
        <source>Client random cannot be combined with ECH.</source>
        <translation>客户端 random (随机)无法与 ECH 结合使用。</translation>
    </message>
    <message>
        <source>Custom SNI cannot be combined with ECH.</source>
        <translation>自定义 SNI 无法与 ECH 结合使用。</translation>
    </message>
    <message>
        <source>Custom SNI cannot be combined with Disable SNI.</source>
        <translation>“自定义 SNI”不能与“禁用 SNI”结合使用。</translation>
    </message>
    <message>
        <source>Client random and custom SNI cannot be combined with REALITY.</source>
        <translation>“客户端随机”和“自定义 SNI”不能与 REALITY 结合使用。</translation>
    </message>
    <message>
        <source>Client random cannot be combined with the custom uTLS fingerprint.</source>
        <translation>客户端随机不能与自定义 uTLS 指纹结合使用。</translation>
    </message>
    <message numerus="yes">
        <source>The link lists %n more server address(es); only the first one was imported.</source>
        <translation>
            <numerusform>该链接列出了另外 %n 个服务器地址；仅导入了第一个。</numerusform>
        </translation>
    </message>
    <message>
        <source>Missing UUID. TUIC v4 (token authentication) is not supported; only TUIC v5 servers work.</source>
        <translation>缺少 UUID。TUIC v4（令牌认证）不被支持；只有 TUIC v5 服务器有效。</translation>
    </message>
    <message>
        <source>Unknown packet encoding, so none was used. Use packetaddr or xudp.</source>
        <translation>未知的数据包编码，因此未使用任何编码 (none)。请使用 packetaddr 或 xudp。</translation>
    </message>
    <message>
        <source>sing-box does not support xtls-rprx-vision-udp443; use xtls-rprx-vision, or the Xray VLESS type.</source>
        <translation>sing-box 不支持 xtls-rprx-vision-udp443；请使用 xtls-rprx-vision，或 Xray VLESS 类型。</translation>
    </message>
    <message>
        <source>XTLS Vision works only with TLS or REALITY directly over TCP: turn on TLS and set the network to tcp, or clear the flow.</source>
        <translation>XTLS Vision 与 TLS 或 REALITY 仅直接工作在 TCP 上：开启 TLS 并设置网络为 TCP，或清除 flow 设置。</translation>
    </message>
    <message>
        <source>Unknown security, so auto was used. Use auto, none, zero, aes-128-gcm, chacha20-poly1305 or aes-128-cfb.</source>
        <translation>未知的安全性，所以使用了 auto (自动)。可使用 auto、 none、 zero、 aes-128-gcm、 chacha20-poly1305 或 aes-128-cfb。</translation>
    </message>
    <message>
        <source>Unsupported TLS mode; a VMess link can use only tls or none.</source>
        <translation>不支持的 TLS 模式；VMess 链接只能使用 tls 或 none。</translation>
    </message>
    <message>
        <source>Use three numbers from 0 to 255, for example 12,34,56.</source>
        <translation>使用三个从 0 到 255 的数字，例如 12,34,56。</translation>
    </message>
    <message>
        <source>H%1 and H%2 overlap; each magic header needs its own number or range (unset ones are 1, 2, 3 and 4).</source>
        <translation>H%1 和 H%2 重叠；每个魔法标头都需要自己的编号或范围（未设置的则为 1、2、3 和 4）。</translation>
    </message>
    <message>
        <source>Header protection needs S1 to S4 of at least 12 each; S%1 is %2.</source>
        <translation>标头保护需要 S1 到 S4 每项至少为 12；S%1 是 %2。</translation>
    </message>
    <message>
        <source>Jmin is larger than Jmax, so every junk packet will be Jmin bytes.</source>
        <translation>Jmin 大于 Jmax，所以每个垃圾数据包将是 Jmin 字节。</translation>
    </message>
    <message>
        <source>A tag is missing its closing &gt;.</source>
        <translation>一个标记缺少它的闭合 &gt;。</translation>
    </message>
    <message>
        <source>Empty tag &lt;&gt;. Use &lt;b 0x...&gt;, &lt;r N&gt;, &lt;rc N&gt;, &lt;rd N&gt;, &lt;dz N&gt;, &lt;t&gt;, &lt;d&gt; or &lt;ds&gt;.</source>
        <translation>空标记 &lt;&gt;。使用 &lt;b 0x...&gt;、 &lt;r N&gt;、 &lt;rc N&gt;、 &lt;rd N&gt;、 &lt;dz N&gt;、 &lt;t&gt;、 &lt;d&gt; 或 &lt;ds&gt;。</translation>
    </message>
    <message>
        <source>&lt;b&gt; needs an even number of hex digits, for example &lt;b 0x1a2b&gt;.</source>
        <translation>&lt;b&gt; 需要偶数个十六进制数字，例如 &lt;b 0x1a2b&gt;。</translation>
    </message>
    <message>
        <source>&lt;%1&gt; needs a whole number of bytes, for example &lt;%1 16&gt;.</source>
        <translation>&lt;%1&gt; 需要一个整数字节，例如 &lt;%1 16&gt;。</translation>
    </message>
    <message>
        <source>Unknown tag &lt;%1&gt;. Use &lt;b 0x...&gt;, &lt;r N&gt;, &lt;rc N&gt;, &lt;rd N&gt;, &lt;dz N&gt;, &lt;t&gt;, &lt;d&gt; or &lt;ds&gt;.</source>
        <translation>未知标记 &lt;%1&gt;。使用 &lt;b 0x...&gt;、&lt;r N&gt;、&lt;rc N&gt;、&lt;rd N&gt;、&lt;dz N&gt;、&lt;t&gt;、&lt;d&gt; 或 &lt;ds&gt;。</translation>
    </message>
    <message>
        <source>Unknown setting, ignored.</source>
        <translation>未知设置，已忽略。</translation>
    </message>
    <message>
        <source>Only the last of the %1 [Peer] sections is used.</source>
        <translation>只有最后的 %1[Peer] 节被使用。</translation>
    </message>
    <message>
        <source>Ignored; Throne sends all traffic of this profile through the tunnel.</source>
        <translation>已忽略；Throne 通过隧道发送这个配置档的所有流量。</translation>
    </message>
    <message>
        <source>The [Peer] section has no Endpoint such as vpn.example.com:51820.</source>
        <translation>[Peer] 节没有类似 vpn.example.com:51820 这样的端点。</translation>
    </message>
    <message>
        <source>Only the first of the %1 peers is used.</source>
        <translation>仅使用第一个  %1 对等节点。</translation>
    </message>
    <message>
        <source>Write none when the server does not use VLESS encryption.</source>
        <translation>当服务器不使用 VLESS 加密时，写入 none。</translation>
    </message>
    <message>
        <source>Must be none, or the mlkem768x25519plus string the server gives.</source>
        <translation>必须为 none，或者服务器提供的 mlkem768x25519plus 字符串。</translation>
    </message>
    <message>
        <source>The part after mlkem768x25519plus must be native, xorpub or random.</source>
        <translation>mlkem768x25519plus 之后的部分必须是 native、xorpub 或 random。</translation>
    </message>
    <message>
        <source>The part after the mode must be 1rtt or 0rtt.</source>
        <translation>mode (模式)之后的部分必须是 1rtt 或 0rtt。</translation>
    </message>
    <message>
        <source>Each key in the string must be URL-safe base64 of 32 or 1184 bytes, without padding.</source>
        <translation>字符串中的每个钥匙必须是 32 或 1184 字节的 URL 安全的 base64，不带填充。</translation>
    </message>
    <message>
        <source>XTLS Vision needs the raw network with TLS or REALITY, or VLESS encryption: change the network or security, or clear the flow.</source>
        <translation>XTLS Vision 需要搭配 TLS 或 REALITY，或 VLESS 加密的原始网络: 更改 network (网络)或 security (安全性)，或清除 flow (流)设置。</translation>
    </message>
    <message>
        <source>XDRIVE needs VLESS encryption: enter the mlkem768x25519plus string the server gives.</source>
        <translation>XDRIVE 需要 VLESS 加密: 输入服务器提供的 mlkem768x25519plus 字符串。</translation>
    </message>
    <message>
        <source>XDRIVE does not support a flow; clear it.</source>
        <translation>XDRIVE 不支持 flow；清除它。</translation>
    </message>
    <message>
        <source>XDRIVE needs Mux: set it to On or Keep Default.</source>
        <translation>XDRIVE 需要 Mux: 设置它为 On 或 Keep Default （及开启或保持默认）。</translation>
    </message>
    <message>
        <source>Expected a whole number; ignored.</source>
        <translation>预期为整数；已忽略。</translation>
    </message>
    <message>
        <source>Expected true or false; ignored.</source>
        <translation>预期为 true 或 false；已忽略。</translation>
    </message>
    <message>
        <source>Could not download the subscription: %1</source>
        <translation>无法下载订阅: %1</translation>
    </message>
    <message>
        <source>The server returned a web page instead of a subscription.</source>
        <translation>服务器返回了一个网页而不是订阅。</translation>
    </message>
    <message>
        <source>New subscription: %1</source>
        <translation>新订阅: %1</translation>
    </message>
    <message>
        <source>Update of %1</source>
        <translation>「%1」的更新</translation>
    </message>
    <message>
        <source>Import</source>
        <translation>导入</translation>
    </message>
    <message>
        <source>Could not copy %1.</source>
        <translation>无法复制 %1。</translation>
    </message>
    <message numerus="yes">
        <source>%n profile(s) could not be saved to the database.</source>
        <translation>
            <numerusform>%n 个配置档无法保存到数据库。</numerusform>
        </translation>
    </message>
    <message>
        <source>No profiles were found.</source>
        <translation>找不到配置档。</translation>
    </message>
    <message>
        <source>The group %1 was created, but its first download failed. Update it to try again; if the download keeps failing, try another TLS version, HTTP version or User Agent in the group&apos;s Advanced settings.</source>
        <translation>已创建了分组 %1，但首次下载失败。请更新它以再次尝试；如果下载持续失败，请在分组的「高级」设置中尝试其他 TLS 版本、HTTP 版本或用户代理。</translation>
    </message>
    <message>
        <source>Problems:</source>
        <translation>问题:</translation>
    </message>
    <message numerus="yes">
        <source>Imported %n profile(s).</source>
        <translation>
            <numerusform>已导入 %n 个配置档。</numerusform>
        </translation>
    </message>
    <message numerus="yes">
        <source>%n could not be imported.</source>
        <translation>
            <numerusform>%n 无法被导入。</numerusform>
        </translation>
    </message>
    <message numerus="yes">
        <source>%n imported with warnings.</source>
        <translation>
            <numerusform>%n 项已导入但带警告。</numerusform>
        </translation>
    </message>
    <message>
        <source>%1 (%2×, e.g. %3)</source>
        <translation>%1 (%2×, 例如 %3)</translation>
    </message>
    <message>
        <source>%1 (%2×)</source>
        <translation>%1 (%2×)</translation>
    </message>
    <message>
        <source>%1 (%2)</source>
        <translation>%1 (%2)</translation>
    </message>
    <message>
        <source>[error]</source>
        <translation>[错误]</translation>
    </message>
    <message>
        <source>[warning]</source>
        <translation>[警告]</translation>
    </message>
    <message>
        <source>See the details below.</source>
        <translation>请查阅以下详细信息。</translation>
    </message>
    <message>
        <source>sing-box endpoint #%1</source>
        <translation>sing-box endpoint #%1</translation>
    </message>
    <message>
        <source>sing-box endpoint &quot;%1&quot;</source>
        <translation>sing-box 端点 &quot;%1&quot;</translation>
    </message>
    <message>
        <source>sing-box outbound #%1</source>
        <translation>sing-box outbound #%1</translation>
    </message>
    <message>
        <source>sing-box outbound &quot;%1&quot;</source>
        <translation>sing-box outbound &quot;%1&quot;</translation>
    </message>
    <message>
        <source>Xray outbound #%1</source>
        <translation>Xray outbound #%1</translation>
    </message>
    <message>
        <source>Xray outbound &quot;%1&quot;</source>
        <translation>Xray outbound &quot;%1&quot;</translation>
    </message>
    <message>
        <source>Xray config #%1</source>
        <translation>Xray 配置 #%1</translation>
    </message>
    <message>
        <source>Xray config &quot;%1&quot;</source>
        <translation>Xray 配置 &quot;%1&quot;</translation>
    </message>
    <message>
        <source>Clash proxy #%1</source>
        <translation>Clash 代理 #%1</translation>
    </message>
    <message>
        <source>Clash proxy &quot;%1&quot;</source>
        <translation>Clash 代理 &quot;%1&quot;</translation>
    </message>
    <message>
        <source>A VLESS profile cannot store this setting, so it was ignored. Such an outbound is kept as written only when nothing else in it needs fixing.</source>
        <translation>VLESS 配置档无法存储这个设置，因此已被忽略。只有当其中没有其他需修复的内容时，这样的 outbound (出站连接)才会按书写原样保留。</translation>
    </message>
    <message>
        <source>Rename this key to &quot;%1&quot;: Throne reads only that spelling, and dropping its value would weaken the connection&apos;s security.</source>
        <translation>重命名这个键为 &quot;%1&quot;: Throne 只读取该拼写，而丢弃其值会削弱连接的安全性。</translation>
    </message>
    <message>
        <source>Rename this key to &quot;%1&quot;: Throne reads only that spelling, so its value was ignored.</source>
        <translation>重命名这个键为 &quot;%1&quot;: Throne 只读取该拼写，因此其值已被忽略。</translation>
    </message>
    <message>
        <source>Could not be parsed.</source>
        <translation>无法解析。</translation>
    </message>
    <message>
        <source>Not valid JSON: %1 at line %2, column %3.</source>
        <translation>不是有效的 JSON: %1 位于第 %2 行，第 %3 列。</translation>
    </message>
    <message>
        <source>Empty object.</source>
        <translation>空白对象。</translation>
    </message>
    <message>
        <source>Missing &quot;type&quot;.</source>
        <translation>缺少 &quot;type&quot;。</translation>
    </message>
    <message>
        <source>Unsupported sing-box type &quot;%1&quot;.</source>
        <translation>不支持的 sing-box 类型 &quot;%1&quot;。</translation>
    </message>
    <message>
        <source>Imported as a custom Xray outbound to keep %1, which a VLESS profile would drop.</source>
        <translation>作为自定义 Xray 出站导入以保留 %1，VLESS 配置档会丢弃该项。</translation>
    </message>
    <message>
        <source>SIP008 server #%1</source>
        <translation>SIP008 服务器 #%1</translation>
    </message>
    <message>
        <source>Empty server object.</source>
        <translation>空服务器对象。</translation>
    </message>
    <message>
        <source>This Clash config only lists proxy-providers, which are not downloaded. Add each provider&apos;s URL as a subscription instead.</source>
        <translation>这个 Clash 配置仅列出了 proxy-providers (代理提供者)，而它们未被下载。请改为将每个提供者的 URL 作为订阅添加。</translation>
    </message>
    <message>
        <source>This Clash config has no proxies list.</source>
        <translation>这个 Clash 配置没有代理列表。</translation>
    </message>
    <message>
        <source>Could not be read: %1</source>
        <translation>无法读取: %1</translation>
    </message>
    <message>
        <source>Could not be read.</source>
        <translation>无法读取。</translation>
    </message>
    <message>
        <source>Could not read the Clash config: %1.</source>
        <translation>无法读取 Clash 配置: %1。</translation>
    </message>
    <message>
        <source>Could not read the Clash config.</source>
        <translation>无法读取 Clash 配置。</translation>
    </message>
    <message>
        <source>Unsupported Clash type &quot;%1&quot;.</source>
        <translation>不支持的 Clash 类型 &quot;%1&quot;。</translation>
    </message>
    <message>
        <source>The [Interface] section has no PrivateKey.</source>
        <translation>[Interface] 节没有 PrivateKey。</translation>
    </message>
    <message>
        <source>The [Peer] section has no PublicKey.</source>
        <translation>[Peer] 节没有 PublicKey。</translation>
    </message>
    <message>
        <source>WireGuard config</source>
        <translation>WireGuard 配置</translation>
    </message>
    <message>
        <source>OpenVPN config</source>
        <translation>OpenVPN 配置</translation>
    </message>
    <message>
        <source>OpenConnect profile</source>
        <translation>OpenConnect 配置档</translation>
    </message>
    <message>
        <source>Write the link type in lower case: &quot;%1://&quot;, not &quot;%2://&quot;.</source>
        <translation>小写书写链接类型: &quot;%1://&quot;，而不是&quot;%2://&quot;。</translation>
    </message>
    <message>
        <source>Unsupported link type &quot;%1://&quot;.</source>
        <translation>不支持的链接类型 &quot;%1://&quot;。</translation>
    </message>
    <message>
        <source>The link is damaged: it is not a valid URL.</source>
        <translation>链接损坏: 它不是有效的 URL。</translation>
    </message>
    <message>
        <source>The link is too long.</source>
        <translation>该链接太长。</translation>
    </message>
    <message>
        <source>Not a dnst:// link.</source>
        <translation>不是 dnst:// 链接。</translation>
    </message>
    <message>
        <source>The link is damaged: its payload is not valid base64.</source>
        <translation>链接损坏: 其有效载荷不是有效的 base64。</translation>
    </message>
    <message>
        <source>The link is damaged: its payload is not a JSON object.</source>
        <translation>链接损坏: 其载荷不是 JSON 对象。</translation>
    </message>
    <message>
        <source>The link does not name the tunnel type.</source>
        <translation>该链接未指明隧道类型。</translation>
    </message>
    <message>
        <source>The SSH private key in the link could not be read; it was ignored.</source>
        <translation>无法读取链接中的 SSH 私钥；它已被忽略。</translation>
    </message>
    <message>
        <source>Enter the tunnel domain, for example t.example.com.</source>
        <translation>输入隧道域名，例如 t.example.com。</translation>
    </message>
    <message>
        <source>Use the domain name delegated to the tunnel server, for example t.example.com.</source>
        <translation>使用分配给隧道服务器的域名，例如 t.example.com。</translation>
    </message>
    <message>
        <source>Unsupported profile type &quot;%1&quot;.</source>
        <translation>不支持的配置档类型 &quot;%1&quot;。</translation>
    </message>
    <message>
        <source>The vpn:// link could not be decoded.</source>
        <translation>无法解码 vpn:// 链接。</translation>
    </message>
    <message>
        <source>No importable profile was found in the vpn:// link.</source>
        <translation>在 vpn:// 链接中未找到可导入的配置档。</translation>
    </message>
    <message>
        <source>Remote DNS &quot;%1&quot;: %2</source>
        <translation>远程 DNS &quot;%1&quot;: %2</translation>
    </message>
    <message>
        <source>Direct DNS &quot;%1&quot;: %2</source>
        <translation>直连 DNS &quot;%1&quot;: %2</translation>
    </message>
    <message>
        <source>The default DNS server must be remote or direct, not &quot;%1&quot;.</source>
        <translation>默认 DNS 服务器必须为 remote (远程)或 direct (直连)，而不是 &quot;%1&quot;。</translation>
    </message>
    <message>
        <source>The profile&apos;s own DNS settings were dropped, so it uses the global ones: %1</source>
        <translation>配置档自身的 DNS 设置已丢弃，因此使用全局的 DNS 设置: %1</translation>
    </message>
    <message>
        <source>%1: %2</source>
        <translation>%1: %2</translation>
    </message>
    <message>
        <source>%1 (&quot;%2&quot;): %3</source>
        <translation>%1 (&quot;%2&quot;): %3</translation>
    </message>
    <message>
        <source>Rule &quot;%1&quot;: %2</source>
        <translation>规则 &quot;%1&quot;: %2</translation>
    </message>
    <message>
        <source>Rule &quot;%1&quot; was dropped: %2</source>
        <translation>规则 &quot;%1&quot; 已丢弃: %2</translation>
    </message>
    <message>
        <source>Rule &quot;%1&quot;: removed the item %2</source>
        <translation>规则 &quot;%1&quot;: 已移除项目 %2</translation>
    </message>
    <message>
        <source>Rule &quot;%1&quot; now uses the proxy outbound: %2</source>
        <translation>规则 “%1” 现在使用代理出站: %2</translation>
    </message>
    <message>
        <source>Rule &quot;%1&quot;: %2 was cleared: %3</source>
        <translation>规则 &quot;%1&quot;: %2 已被清除: %3</translation>
    </message>
    <message>
        <source>Rule &quot;%1&quot;: the outbound is neither a profile name nor an id, so it uses proxy.</source>
        <translation>规则 &quot;%1&quot;: outbound (出站)既不是配置档名称也不是 ID，因此它使用代理。</translation>
    </message>
    <message>
        <source>Rule &quot;%1&quot; was dropped: %2 holds something other than text, numbers or booleans.</source>
        <translation>规则 &quot;%1&quot; 被丢弃: %2 含有文本、数字或布尔值之外的内容。</translation>
    </message>
    <message>
        <source>Rule &quot;%1&quot;: %2 holds something other than text, numbers or booleans, so it was ignored.</source>
        <translation>规则“%1”: %2 含有一些文本、数字或布尔值之外的内容，因此被忽略。</translation>
    </message>
    <message>
        <source>Rule &quot;%1&quot;: %2 takes one value, so only &quot;%3&quot; was kept.</source>
        <translation>规则 &quot;%1&quot;: %2 只接受一个值，因此只保留了 &quot;%3&quot;。</translation>
    </message>
    <message>
        <source>%1 Shared endpoints arrive without their keys and passwords.</source>
        <translation>%1 个共享端点到达时未带其钥匙和密码。</translation>
    </message>
    <message>
        <source>The shared endpoint %1 was not imported: %2</source>
        <translation>共享端点 %1 未被导入: %2</translation>
    </message>
    <message>
        <source>The shared endpoint %1 was imported with a warning: %2</source>
        <translation>共享端点 %1 导入时带警告: %2</translation>
    </message>
    <message>
        <source>The default outbound &quot;%1&quot; is unknown, so proxy is used.</source>
        <translation>默认出站 &quot;%1&quot; 是未知的，因此使用代理。</translation>
    </message>
    <message>
        <source>Rule #%1 is not a JSON object and was skipped.</source>
        <translation>规则 #%1 不是 JSON 对象并已被跳过。</translation>
    </message>
    <message>
        <source>Line %1 of the %2 rules (&quot;%3&quot;): %4</source>
        <translation>%2 规则的第 %1 行 (&quot;%3&quot;): %4</translation>
    </message>
    <message>
        <source>Start the line with domain:, suffix:, keyword:, regex:, ruleset:, ip:, processName: or processPath:.</source>
        <translation>以 domain:、suffix:、keyword:、regex:、ruleset:、ip:、processName: 或 processPath: 开头的行。</translation>
    </message>
    <message>
        <source>Add a value after &quot;%1:&quot;.</source>
        <translation>在 &quot;%1&quot; 后添加一个值。</translation>
    </message>
    <message>
        <source>Start the line with domain:, suffix:, keyword:, regex:, ruleset: or ip:.</source>
        <translation>以 domain:, suffix:, keyword:, regex:, ruleset: 或 ip: 开始该行。</translation>
    </message>
    <message>
        <source>Start the line with processName: or processPath:.</source>
        <translation>以 processName: 或 processPath: 开始该行。</translation>
    </message>
    <message>
        <source>Write only the domain, without a scheme such as https://; as written, this never matches.</source>
        <translation>只写域名，不要带如 https:// 之类的方案；按这样写，这条永远不会匹配。</translation>
    </message>
    <message>
        <source>A domain has no spaces in it, so this never matches.</source>
        <translation>域名中没有空格，所以这条永远不会匹配。</translation>
    </message>
    <message>
        <source>Write only the domain, without a path; as written, this never matches.</source>
        <translation>只写域名，不写路径；按这样写，这条永不匹配。</translation>
    </message>
    <message>
        <source>A domain has no port or &apos;:&apos; in it, so this never matches; match ports and IP addresses with conditions of their own.</source>
        <translation>一个域名中没有端口或 ‘:’ 在其中，所以这条永不匹配；用它们自身的条件匹配端口和 IP 地址。</translation>
    </message>
    <message>
        <source>The core compares lowercase host names, so this never matches; write it in lowercase.</source>
        <translation>该核心比较小写主机名，所以这条永不匹配；请将其写成小写。</translation>
    </message>
    <message>
        <source>Write an IPv6 address as 2001:db8::1 or [2001:db8::1]; a port goes in override_port.</source>
        <translation>IPv6 地址写为 2001:db8::1 或 [2001:db8::1]；端口写在 override_port 中。</translation>
    </message>
    <message>
        <source>Put the port in override_port and only the address here, such as 1.2.3.4 or example.com.</source>
        <translation>端口放在 override_port 中，这里只填写地址，如 1.2.3.4 或 example.com。</translation>
    </message>
    <message>
        <source>An IP address sends no server name; use a host name such as www.example.com.</source>
        <translation>IP 地址不会发送服务器名称；请使用主机名，譬如 www.example.com。</translation>
    </message>
    <message>
        <source>Write only the host name, without a port.</source>
        <translation>仅写主机名，不带端口。</translation>
    </message>
    <message>
        <source>Not a valid outbound; pick another one.</source>
        <translation>不是有效的出站；请选择另一个。</translation>
    </message>
    <message>
        <source>The outbound profile no longer exists; pick another one.</source>
        <translation>出站配置档不再存在；请选取另一个。</translation>
    </message>
    <message>
        <source>Routing rules cannot use &quot;%1&quot;: extra core profiles and full configs are not supported. Pick another outbound.</source>
        <translation>路由规则不能使用“%1”：不支持额外核心配置档和完整配置。请选取其他出站。</translation>
    </message>
    <message>
        <source>The chain &quot;%1&quot; has no hops; pick another outbound.</source>
        <translation>链 &quot;%1&quot; 没有跳点；请选取另一个出站。</translation>
    </message>
    <message>
        <source>Routing rules cannot use the chain &quot;%1&quot;: a hop of it is missing, another chain, an extra core profile or a full config. Pick another outbound.</source>
        <translation>路由规则不能使用链 “%1”: 它缺少其中一个跳点、(引用了)另一条链、(包含)一个额外核心配置档或(使用了)完整配置。请选择其他出站。</translation>
    </message>
    <message>
        <source>A range goes in port_range, written like 1000:2000.</source>
        <translation>（端口）范围放在 port_range 中，写法如 1000:2000。</translation>
    </message>
    <message>
        <source>A range goes in source_port_range, written like 1000:2000.</source>
        <translation>（端口）范围放在 source_port_range 中，写法如 1000:2000。</translation>
    </message>
    <message>
        <source>Write only the program&apos;s file name, such as chrome.exe; a name with a path never matches. Match a full path with a process path condition instead.</source>
        <translation>只写程序的文件名，例如 chrome.exe；带路径的名称永远不会匹配。要匹配完整路径，请改用“进程路径”条件。</translation>
    </message>
    <message>
        <source>Write the full path, such as C:\Program Files\App\app.exe or /usr/bin/app.</source>
        <translation>填写完整路径，例如 C:\Program Files\App\app.exe 或 /usr/bin/app。</translation>
    </message>
    <message>
        <source>Unknown rule-set. Use a built-in name such as geosite-google, or an http(s) URL of a binary .srs file.</source>
        <translation>未知规则集。使用内置名称，如 geosite-google，或二进制 .srs 文件的 http(s) URL。</translation>
    </message>
    <message>
        <source>A route-options rule cannot turn on both tls_fragment and tls_record_fragment; keep one, or use a route rule, which accepts both.</source>
        <translation>oute-options (路由选项)规则不能同时开启 tls_fragment 和 tls_record_fragment；保留其中一个，或使用可同时接受两者的路由规则。</translation>
    </message>
    <message>
        <source>A route-options rule must set at least one option: override_address, override_port, tls_fragment, tls_record_fragment or tls_spoof.</source>
        <translation>oute-options (路由选项)规则不能同时开启 tls_fragment 和 tls_record_fragment；保留其中一个，或者使用可同时接受两者的路由规则。</translation>
    </message>
    <message>
        <source>Has no effect unless tls_fragment is on.</source>
        <translation>无效果，除非 tls_fragment 开启。</translation>
    </message>
    <message>
        <source>Has no effect without a tls_spoof host name.</source>
        <translation>没有 tls_spoof 主机名则无效果。</translation>
    </message>
    <message>
        <source>Has no effect with the drop method.</source>
        <translation>带 drop 方法则无效。</translation>
    </message>
    <message>
        <source>A %1 rule ignores it; it applies only to these actions: %2.</source>
        <translation>一条 %1 规则会忽略它；它仅适用于这些动作: %2。</translation>
    </message>
    <message>
        <source>This rule has no match conditions, so it matches all traffic.</source>
        <translation>这条规则没有匹配条件，因此它匹配所有流量。</translation>
    </message>
    <message>
        <source>Unnamed entry</source>
        <translation>未命名条目</translation>
    </message>
    <message>
        <source>%1: algorithm &quot;%2&quot; is not supported (only SHA1, SHA256 and SHA512), so its codes would be wrong.</source>
        <translation>%1: 算法 &quot;%2&quot; 不被支持（仅支持 SHA1、SHA256 和 SHA512），所以其代码将是错误的。</translation>
    </message>
    <message>
        <source>%1: %2 digits are not supported (use %3 to %4).</source>
        <translation>%1: 不支持 %2 位数字（使用 %3 到 %4）。</translation>
    </message>
    <message>
        <source>%1: a period of &quot;%2&quot; seconds is not supported (use %3 to %4).</source>
        <translation>%1: 不支持 “%2” 秒的时间段（请使用 %3 到 %4）。</translation>
    </message>
    <message>
        <source>%1: the counter &quot;%2&quot; is not a whole number of 0 or more.</source>
        <translation>%1: 计数器 “%2” 不是 0 或更大的整数。</translation>
    </message>
    <message>
        <source>%1: type &quot;%2&quot; is not supported (only totp and hotp).</source>
        <translation>%1: 类型 “%2” 不被支持（仅支持 totp 和 hotp）。</translation>
    </message>
    <message>
        <source>%1: there is no secret.</source>
        <translation>%1: 没有密钥。</translation>
    </message>
    <message>
        <source>%1: the secret is not valid base32 (letters A-Z and digits 2-7).</source>
        <translation>%1: 该密钥不是有效的 base32 （字母 A-Z 和数字 2-7）。</translation>
    </message>
    <message>
        <source>%1: the digit count code %2 is not supported.</source>
        <translation>%1: 不支持数字计数代码 %2。</translation>
    </message>
    <message>
        <source>Skipped an item that is not an OTP entry.</source>
        <translation>跳过了不是 OTP 条目的项目。</translation>
    </message>
    <message>
        <source>An otpauth link could not be read.</source>
        <translation>无法读取 otpauth 链接。</translation>
    </message>
    <message>
        <source>Skipped a base32 secret shorter than %1 characters.</source>
        <translation>跳过少于 %1 个字符的 base32 密钥。</translation>
    </message>
    <message>
        <source>Skipped a secret that is not valid base32 (letters A-Z and digits 2-7).</source>
        <translation>跳过了无效的 base32 的密钥（字母 A–Z 和数字 2–7）。</translation>
    </message>
    <message>
        <source>Not a valid IPv4 address (four numbers from 0 to 255 separated by dots).</source>
        <translation>不是有效的 IPv4 地址（四个用点间隔的从 0 到 255 的数字）。</translation>
    </message>
    <message>
        <source>Not a valid IPv6 address.</source>
        <translation>不是有效的 IPv6 地址。</translation>
    </message>
    <message>
        <source>Not a valid IP address.</source>
        <translation>不是有效的 IP 地址。</translation>
    </message>
    <message>
        <source>An IPv6 zone (the part starting with %) is not allowed here.</source>
        <translation>这里不允许使用 IPv6 区域（以 % 开头的部分）。</translation>
    </message>
    <message>
        <source>Must be a port number from 1 to 65535.</source>
        <translation>必须是从 1 到 65535 之间的端口号。</translation>
    </message>
    <message>
        <source>Not a valid host name or IP address.</source>
        <translation>不是有效的主机名或 IP 地址。</translation>
    </message>
    <message>
        <source>Enter only the host name, without a scheme such as https://.</source>
        <translation>仅输入主机名，不带如 https:// 之类的协议方案。</translation>
    </message>
    <message>
        <source>Must be from %1 to %2.</source>
        <translation>必须是从 %1 到 %2 之间。</translation>
    </message>
    <message>
        <source>The core&apos;s regex engine (RE2) does not support &quot;%1&quot;.</source>
        <translation>核心的正则表达式引擎不支持 &quot;%1&quot;。</translation>
    </message>
    <message>
        <source>The core&apos;s regex engine (RE2) allows at most 1000 repetitions, counting nested repeats multiplied together.</source>
        <translation>核心的正则表达式引擎（RE2）最多允许 1000 次重复，计算嵌套重复时将它们相乘。</translation>
    </message>
    <message>
        <source>This field is required.</source>
        <translation>这个字段是必需的。</translation>
    </message>
    <message>
        <source>Must be a whole number.</source>
        <translation>必须是整数。</translation>
    </message>
    <message>
        <source>Use a range like 20000:30000.</source>
        <translation>使用类似 20000:30000 的范围。</translation>
    </message>
    <message>
        <source>Use a range like 20000-30000.</source>
        <translation>使用类似 20000-30000 的范围。</translation>
    </message>
    <message>
        <source>Ports must be at most 65535.</source>
        <translation>端口至多是 65535。</translation>
    </message>
    <message>
        <source>&quot;%1&quot; is a single port; write it as %1:%1.</source>
        <translation>&quot;%1&quot; 是单一端口；写成 %1:%1。</translation>
    </message>
    <message>
        <source>&quot;%1&quot; is a single port; write it as %1-%1.</source>
        <translation>&quot;%1&quot; 是单一端口；写成 %1-%1。</translation>
    </message>
    <message>
        <source>The range starts after it ends, so it matches nothing.</source>
        <translation>范围开始在其结束之后，所以它匹配不到任何东西。</translation>
    </message>
    <message>
        <source>Must be a whole number or a range like 10-20.</source>
        <translation>必须是一个整数或像 10-20 这样的范围。</translation>
    </message>
    <message>
        <source>The range starts after it ends; put the smaller number first.</source>
        <translation>范围开始在其结束之后，先放置较小的数。</translation>
    </message>
    <message>
        <source>Add a unit: write %1ms or %1s, not %1.</source>
        <translation>添加单位: 写成 %1ms 或 %1s，而不是 %1。</translation>
    </message>
    <message>
        <source>Not a duration. Use a number followed by a unit (ns, us, ms, s, m, h or d), for example 500ms or 1m30s.</source>
        <translation>不是时长。请使用后跟单位（ns、us、ms、s、m、h 或 d）的数字，例如 500ms 或 1m30s。</translation>
    </message>
    <message>
        <source>Use true or false.</source>
        <translation>使用 true 或 false。</translation>
    </message>
    <message>
        <source>This DNS tunnel type is not supported.</source>
        <translation>这个 DNS 隧道类型不被支持。</translation>
    </message>
    <message>
        <source>The link does not name the service behind the tunnel.</source>
        <translation>该链接未指明隧道背后的服务。</translation>
    </message>
    <message>
        <source>This service behind the tunnel is not supported.</source>
        <translation>隧道背后的这个服务不被支持。</translation>
    </message>
    <message>
        <source>The resolver&apos;s transport is unknown, so it is used over UDP.</source>
        <translation>解析器的传输协议未知，因此通过 UDP 使用。</translation>
    </message>
    <message>
        <source>This SlipNet link version is not supported.</source>
        <translation>这个 SlipNet 链接版本不被支持。</translation>
    </message>
    <message>
        <source>The link is damaged: it is shorter than its version allows.</source>
        <translation>链接损坏：其长度短于其版本允许范围。</translation>
    </message>
    <message>
        <source>This link comes from a newer SlipNet version; check the imported settings.</source>
        <translation>这个链接来自较新版本的 SlipNet；检查导入的设置。</translation>
    </message>
    <message>
        <source>NoizDNS was imported as plain DNSTT: its server accepts that, but without NoizDNS&apos;s DPI evasion.</source>
        <translation>NoizDNS 已作为普通 DNSTT 导入：其服务器接受这种方式，但不具备 NoizDNS 的 DPI 规避功能。</translation>
    </message>
    <message>
        <source>This SlipNet profile type is not supported.</source>
        <translation>这个 SlipNet 配置档类型不被支持。</translation>
    </message>
    <message>
        <source>Use the server&apos;s public key: 64 hexadecimal digits, as the server prints it.</source>
        <translation>使用服务器的公钥：64 位十六进制数字，按服务器显示使用。</translation>
    </message>
    <message>
        <source>The classic dnstt wire only uses TXT records.</source>
        <translation>经典的 DNSTT 线路格式（传输数据协议）仅使用 TXT 记录。</translation>
    </message>
    <message>
        <source>The classic dnstt wire always uses 8-byte client IDs.</source>
        <translation>经典的 DNSTT 线路格式（传输数据协议）始终使用 8 字节客户端 IDs。</translation>
    </message>
    <message>
        <source>Each query would carry %1 bytes, fewer than the %2 the tunnel needs: use a shorter domain or a longer maximum QNAME length.</source>
        <translation>每个查询将携带 %1 字节，少于隧道所需的 %2 字节：请使用更短的域名，或更长的“最大 QNAME 长度”。</translation>
    </message>
    <message>
        <source>Must be a number.</source>
        <translation>必须是一个数。</translation>
    </message>
    <message>
        <source>Must not be negative.</source>
        <translation>不能为负。</translation>
    </message>
    <message>
        <source>The keepalive interval must be shorter than the idle timeout.</source>
        <translation>“保活时间间隔”必须比“空闲超时”短。</translation>
    </message>
    <message>
        <source>Without a backend, only a profile chained over this one can use the tunnel.</source>
        <translation>没有后端，仅通过这个配置档串联的配置档可以使用该隧道。</translation>
    </message>
    <message>
        <source>Encrypted SlipNet links cannot be read: export the profile from SlipNet without encryption.</source>
        <translation>无法读取加密的 SlipNet 链接：从 SlipNet 不加密导出配置档。</translation>
    </message>
    <message>
        <source>Use only one of socks, shadowsocks and ssh.</source>
        <translation>仅使用 socks、 shadowsocks 和 ssh 其中一种。</translation>
    </message>
    <message>
        <source>Use whole seconds, for example 30s.</source>
        <translation>使用整数秒，例如 30s。</translation>
    </message>
    <message>
        <source>Must be at least %1.</source>
        <translation>必须至少 %1。</translation>
    </message>
    <message>
        <source>Must be at most %1.</source>
        <translation>必须至多 %1。</translation>
    </message>
    <message>
        <source>Use a whole number before the unit.</source>
        <translation>在单位前使用一个整数。</translation>
    </message>
    <message>
        <source>The number is too large.</source>
        <translation>该数太大。</translation>
    </message>
    <message>
        <source>Add a unit, for example 8m or 512kb.</source>
        <translation>添加一个单位，例如 8m 或 512kb。</translation>
    </message>
    <message>
        <source>Not a size. Use a whole number followed by a unit, for example 8m or 512kb.</source>
        <translation>不是大小。使用后跟单位的一个整数，例如 8m 或 512kb。</translation>
    </message>
    <message>
        <source>Unknown unit &quot;%1&quot;. Use b, k, kb, m, mb, g or gb.</source>
        <translation>未知单位 &quot;%1&quot;。使用 b, k, kb, m, mb, g 或 gb。</translation>
    </message>
    <message>
        <source>Use an address with a prefix length, for example 10.0.0.0/8 or fd00::/8.</source>
        <translation>使用带前缀长度的地址，例如 10.0.0.0/8 或 fd00::/8。</translation>
    </message>
    <message>
        <source>Use a prefix length instead of a netmask, for example 10.0.0.0/8.</source>
        <translation>使用前缀长度而不是网络掩码，例如 10.0.0.0/8。</translation>
    </message>
    <message>
        <source>Write the prefix length without leading zeros.</source>
        <translation>填写不带前导零的前缀长度。</translation>
    </message>
    <message>
        <source>The prefix length must be from 0 to %1.</source>
        <translation>前缀长度必须在 0 到 %1 之间。</translation>
    </message>
    <message>
        <source>Enter only the host name, without a path or user name.</source>
        <translation>仅输入主机名，不带路径或用户名。</translation>
    </message>
    <message>
        <source>Enter the port in its own field.</source>
        <translation>在它自己的字段中输入端口。</translation>
    </message>
    <message>
        <source>Put an IPv6 address in brackets before the port, for example [2001:db8::1]:443.</source>
        <translation>把 IPv6 地址放在端口前的方括号中，例如 [2001:db8::1]:443。</translation>
    </message>
    <message>
        <source>Add the port after a colon, for example example.com:443.</source>
        <translation>在冒号后添加端口，例如 example.com:443。</translation>
    </message>
    <message>
        <source>Must be a %1-byte key in URL-safe base64 without padding (%2 characters).</source>
        <translation>必须是一个无填充的 URL 安全的 base64 编码的 %1 字节钥匙（%2 个字符）。</translation>
    </message>
    <message>
        <source>Must be a %1-byte key in URL-safe base64 (%2 characters).</source>
        <translation>必须是一个 URL 安全的 base64 编码的 %1 字节钥匙（%2 个字符）。</translation>
    </message>
    <message>
        <source>Must be a %1-byte key in base64 without padding (%2 characters).</source>
        <translation>必须是一个无填充的 base64 编码的 %1 字节钥匙（%2 个字符）。</translation>
    </message>
    <message>
        <source>Must be a %1-byte key in base64 (%2 characters).</source>
        <translation>必须是一个 base64 编码的 %1 字节钥匙（%2 个字符）。</translation>
    </message>
    <message>
        <source>Not valid base64.</source>
        <translation>不是有效的 base64。</translation>
    </message>
    <message>
        <source>Must be hexadecimal (0-9, a-f).</source>
        <translation>必须是十六进制（0-9, a-f）。</translation>
    </message>
    <message>
        <source>Must have an even number of characters.</source>
        <translation>必须是偶数个字符。</translation>
    </message>
    <message>
        <source>Must be at most %1 characters long.</source>
        <translation>必须至多 %1 个字符长。</translation>
    </message>
    <message>
        <source>Must be at least %1 characters long.</source>
        <translation>必须至少有 %1 个字符长。</translation>
    </message>
    <message>
        <source>Must be a UUID, for example 123e4567-e89b-12d3-a456-426614174000.</source>
        <translation>必须是 UUID，例如 123e4567-e89b-12d3-a456-426614174000。</translation>
    </message>
    <message>
        <source>Must be a UUID, or a custom ID of at most 30 bytes.</source>
        <translation>必须是 UUID，或一个最多 30 字节的自定义 ID。</translation>
    </message>
    <message>
        <source>%1 or %2</source>
        <translation>%1 或 %2</translation>
    </message>
    <message>
        <source>Must be a URL starting with %1.</source>
        <translation>必须是以 %1 开头的 URL。</translation>
    </message>
    <message>
        <source>Not a valid URL.</source>
        <translation>不是有效的 URL。</translation>
    </message>
    <message>
        <source>Unknown value &quot;%1&quot;. Use one of: %2.</source>
        <translation>未知值 &quot;%1&quot;。请使用以下之一: %2。</translation>
    </message>
    <message>
        <source>Must not contain spaces or special characters.</source>
        <translation>一定不包含空格或特殊字符。</translation>
    </message>
    <message>
        <source>Invalid regular expression: %1.</source>
        <translation>无效正则表达式: %1。</translation>
    </message>
    <message>
        <source>Not valid JSON: %1 (line %2, column %3).</source>
        <translation>不是有效 JSON: %1 (第 %2 行，第 %3 列)。</translation>
    </message>
    <message>
        <source>Must be a JSON object.</source>
        <translation>必须是 JSON 对象。</translation>
    </message>
    <message>
        <source>Line %1 (&quot;%2&quot;): %3</source>
        <translation>第 %1 行 (&quot;%2&quot;): %3</translation>
    </message>
    <message>
        <source>Fix these before saving:</source>
        <translation>在保存前修正这些:</translation>
    </message>
    <message numerus="yes">
        <source>…and %n more</source>
        <translation>
            <numerusform>…以及其它 %n 项</numerusform>
        </translation>
    </message>
    <message>
        <source>Invalid input</source>
        <translation>无效输入</translation>
    </message>
    <message>
        <source>Warnings:</source>
        <translation>警告:</translation>
    </message>
    <message>
        <source>Use system, an IP address with an optional port, a range such as 192.0.2.0/24, or a udp://, tcp://, tls:// or https:// address.</source>
        <translation>使用系统、一个带可选端口的 IP 地址、一个地址范围（如 192.0.2.0/24），或一个 udp://, tcp://, tls:// 或 https:// 地址。</translation>
    </message>
    <message>
        <source>Use system, an IP address or a range such as 192.0.2.0/24, each with an optional :port.</source>
        <translation>使用系统、一个 IP 地址或一个地址范围（如 192.0.2.0/24），每种都可选带 :端口。</translation>
    </message>
    <message>
        <source>Use an IP address with an optional port, or a udp://, tcp:// or https:// address.</source>
        <translation>使用一个带可选端口的 IP 地址，或者一个 udp://, tcp:// 或 https:// 地址。</translation>
    </message>
    <message>
        <source>Only https:// addresses take a path.</source>
        <translation>仅 https:// 地址可含路径。</translation>
    </message>
    <message>
        <source>XDNS has no system resolver: enter the address of a DNS resolver.</source>
        <translation>XDNS 没有系统解析器：输入 DNS 解析器的地址。</translation>
    </message>
    <message>
        <source>Enter single addresses here; a range works only from an IP list.</source>
        <translation>在这里输入单个地址；地址范围仅通过 IP 列表使用。</translation>
    </message>
    <message>
        <source>Put an IPv6 address in brackets, for example [2001:db8::1]:53.</source>
        <translation>IPv6 地址需放在方括号中，例如 [2001:db8::1]:53。</translation>
    </message>
    <message>
        <source>A range may hold at most %1 addresses: use /%2 or longer for IPv4 and /%3 or longer for IPv6.</source>
        <translation>一个地址范围最多可含 %1 个地址：IPv4 使用 /%2 或更长的前缀，IPv6 使用 /%3 或更长的前缀。</translation>
    </message>
    <message>
        <source>Only system, udp, tcp, tls and https resolvers with a server are supported.</source>
        <translation>仅支持带服务器的 system, udp, tcp, tls 和 https 解析器。</translation>
    </message>
    <message>
        <source>A TLS server name other than the resolver&apos;s own host is not kept.</source>
        <translation>解析器的自身主机以外的 TLS 服务器名称不会被保留。</translation>
    </message>
    <message>
        <source>Only the first %1 resolvers were kept.</source>
        <translation>仅前 %1 个解析器会被保留。</translation>
    </message>
    <message>
        <source>Unknown encryption method: use a number from 0 to 5.</source>
        <translation>未知的加密方法：使用从 0 到 5 之间的数字。</translation>
    </message>
    <message>
        <source>MasterDNS only carries a SOCKS or a custom service.</source>
        <translation>MasterDNS 仅承载 SOCKS 或自定义服务。</translation>
    </message>
    <message>
        <source>The link also carries a proxy to run over the tunnel; import that proxy separately and chain it over this profile.</source>
        <translation>该链接还携带了一个通过隧道运行的代理；请单独导入该代理，并将串联它到这个配置档上。</translation>
    </message>
    <message>
        <source>This MasterDNS link format is not supported.</source>
        <translation>这个 MasterDNS 链接格式不被支持。</translation>
    </message>
    <message>
        <source>This MasterDNS link version is not supported.</source>
        <translation>这个 MasterDNS 链接版本不被支持。</translation>
    </message>
    <message>
        <source>The link is damaged: its client configuration cannot be read.</source>
        <translation>链接损坏：其客户端配置无法被读取。</translation>
    </message>
    <message>
        <source>Unknown protocol type: use SOCKS5 or TCP.</source>
        <translation>未知协议类型：使用 SOCKS5 或 TCP。</translation>
    </message>
    <message>
        <source>Use a number of seconds above 0.</source>
        <translation>使用大于 0 的秒数。</translation>
    </message>
    <message>
        <source>Settings this profile does not support were ignored.</source>
        <translation>这个配置档不支持的设置已被忽略。</translation>
    </message>
    <message>
        <source>Enter the encryption key the server uses.</source>
        <translation>输入服务器使用的加密钥匙。</translation>
    </message>
    <message>
        <source>The minimum MTU %1 is above the maximum %2.</source>
        <translation>最小 MTU %1大于最大 MTU %2。</translation>
    </message>
    <message>
        <source>Routing rules cannot use the chain &quot;%1&quot;: it holds an auto selector, so it can only be started. Pick another outbound.</source>
        <translation>路由规则不能使用链 “%1”: 它包含自动选择器，因此只能被启动。请选择其他出站。</translation>
    </message>
    <message>
        <source>Not a valid regular expression: %1 (at character %2).</source>
        <translation>不是有效的正则表达式: %1 (位于第 %2 个字符)。</translation>
    </message>
    <message>
        <source>Not a two-letter country code: %1. Use codes such as HK,JP to keep only those countries, or !RU to remove one.</source>
        <translation>不是两字母国家代码: %1。使用 HK,JP 之类的代码仅保留这些国家，或使用 !RU 移除一个国家。</translation>
    </message>
    <message>
        <source>%1 is not a valid regular expression, so it was ignored: %2</source>
        <translation>「%1」不是有效的正则表达式，已被忽略: %2</translation>
    </message>
    <message>
        <source>Keep only matching</source>
        <translation>仅保留匹配项</translation>
    </message>
    <message>
        <source>Remove matching</source>
        <translation>移除匹配项</translation>
    </message>
    <message>
        <source>Unknown protocol in Remove types, ignored: %1</source>
        <translation>「移除类型」中的未知协议，已被忽略: %1</translation>
    </message>
    <message>
        <source>Not a two-letter country code, ignored: %1</source>
        <translation>不是两字母国家代码，已被忽略: %1</translation>
    </message>
    <message>
        <source>Use the 32-byte certificate chain hash in base64 (44 characters ending in =), as juicity-server prints it.</source>
        <translation>请使用 base64 格式的 32 字节证书链哈希（44 个字符，以 = 结尾），即 juicity-server 输出的值。</translation>
    </message>
    <message>
        <source>Use either Client Random or Client Random Key, not both.</source>
        <translation>“客户端随机”和“客户端随机密钥”只能设置其中一个。</translation>
    </message>
    <message>
        <source>Client Random Key cannot be combined with Disable SNI.</source>
        <translation>“客户端随机密钥”不能与“禁用 SNI”结合使用。</translation>
    </message>
    <message>
        <source>Client Random Key cannot be combined with mixed-case SNI.</source>
        <translation>“客户端随机密钥”不能与混合大小写的 SNI 结合使用。</translation>
    </message>
    <message>
        <source>Client Random Key needs a domain name as the SNI; set Custom SNI or the TLS server name.</source>
        <translation>“客户端随机密钥”要求 SNI 为域名；请设置“自定义 SNI”或 TLS 服务器名称。</translation>
    </message>
    <message>
        <source>The certificate chain pin replaces certificate verification; clear the TLS certificate and certificate pins.</source>
        <translation>证书链固定会取代证书验证；请清除 TLS 证书和证书固定。</translation>
    </message>
</context>
<context>
    <name>XrayMuxSettingsWidget</name>
    <message>
        <source>Concurrency</source>
        <translation>并发数</translation>
    </message>
    <message>
        <source>&lt;html&gt;&lt;head/&gt;&lt;body&gt;&lt;p&gt;Streams one connection carries before another opens. 0 uses Xray&apos;s 8, and -1 sends TCP outside the mux.&lt;/p&gt;&lt;/body&gt;&lt;/html&gt;</source>
        <translation>&lt;html&gt;&lt;head/&gt;&lt;body&gt;&lt;p&gt;在开启另一个连接前一个连接承载的流数量。为 0 时使用 Xray 的 8，而为 -1 时则在 mux (多路复用)外发送 TCPS（就是TCP流量不经mux）。&lt;/p&gt;&lt;/body&gt;&lt;/html&gt;</translation>
    </message>
    <message>
        <source>No limit</source>
        <translation>无限制</translation>
    </message>
    <message>
        <source>Max Connections</source>
        <translation>最大连接数</translation>
    </message>
    <message>
        <source>&lt;html&gt;&lt;head/&gt;&lt;body&gt;&lt;p&gt;Keep at most this many connections, for TCP and for XUDP each. Once every one carries Concurrency streams, new streams join the least busy one.&lt;/p&gt;&lt;/body&gt;&lt;/html&gt;</source>
        <translation>&lt;html&gt;&lt;head/&gt;&lt;body&gt;&lt;p&gt;对于 TCP 和 XUDP，各自至多保留这么多个连接数。一旦每个连接都承载了“并发数”(Concurrency)个流时，新流会加入当前负载最低的连接。&lt;/p&gt;&lt;/body&gt;&lt;/html&gt;</translation>
    </message>
    <message>
        <source>XUDP Concurrency</source>
        <translation>XUDP 并发</translation>
    </message>
    <message>
        <source>&lt;html&gt;&lt;head/&gt;&lt;body&gt;&lt;p&gt;UDP streams per connection, on connections of their own. 0 carries UDP on the TCP connections, and -1 sends it outside the mux.&lt;/p&gt;&lt;/body&gt;&lt;/html&gt;</source>
        <translation>&lt;html&gt;&lt;head/&gt;&lt;body&gt;&lt;p&gt;每个连接上的 UDP 流数量，这些流都在它们自有的连接上。为 0 时，在 TCP 连接上承载 UDP；为 -1 时，在 mux (多路复用)外发送 UDP 流（即 UDP 流不经 mux）。&lt;/p&gt;&lt;/body&gt;&lt;/html&gt;</translation>
    </message>
    <message>
        <source>Reject</source>
        <translation>拒绝</translation>
    </message>
    <message>
        <source>Allow</source>
        <translation>允许</translation>
    </message>
    <message>
        <source>Skip</source>
        <translation>跳过</translation>
    </message>
    <message>
        <source>UDP/443</source>
        <translation>UDP/443</translation>
    </message>
    <message>
        <source>&lt;html&gt;&lt;head/&gt;&lt;body&gt;&lt;p&gt;UDP to port 443, mostly QUIC. &lt;b&gt;Reject&lt;/b&gt; makes browsers fall back to TCP (Xray&apos;s default), &lt;b&gt;Allow&lt;/b&gt; carries it through the mux, and &lt;b&gt;Skip&lt;/b&gt; sends it outside the mux.&lt;/p&gt;&lt;/body&gt;&lt;/html&gt;</source>
        <translation>&lt;html&gt;&lt;head/&gt;&lt;body&gt;&lt;p&gt;UDP 送到端口 443，通常是 QUIC。&lt;b&gt;拒绝&lt;/b&gt; 使浏览器回退到 TCP (Xray 的默认行为)，&lt;b&gt;允许&lt;/b&gt; 通过 mux (多路复用)承载这类流量，而&lt;b&gt;跳过&lt;/b&gt;则在多路复用外发送这类流量。&lt;/p&gt;&lt;/body&gt;&lt;/html&gt;</translation>
    </message>
    <message>
        <source>Custom Mux</source>
        <translation>自定义多路复用</translation>
    </message>
</context>
<context>
    <name>Sys::CoreElevation</name>
    <message>
        <source>The Windows administrator prompt was declined or could not be shown.</source>
        <translation>Windows 管理员确认被拒绝或无法显示。</translation>
    </message>
    <message>
        <source>Done.</source>
        <translation>完成。</translation>
    </message>
    <message>
        <source>The helper did not receive administrator rights.</source>
        <translation>辅助程序未获得管理员权限。</translation>
    </message>
    <message>
        <source>Windows asked for a different administrator account; the grant must come from your own account.</source>
        <translation>Windows 请求了另一个管理员帐户；必须由您自己的帐户授予。</translation>
    </message>
    <message>
        <source>The helper was started with invalid arguments.</source>
        <translation>辅助程序的启动参数无效。</translation>
    </message>
    <message>
        <source>Could not copy ThroneCore into its protected folder.</source>
        <translation>无法将 ThroneCore 复制到受保护的文件夹。</translation>
    </message>
    <message>
        <source>Could not register the scheduled task that starts ThroneCore as administrator.</source>
        <translation>无法注册以管理员身份启动 ThroneCore 的计划任务。</translation>
    </message>
    <message>
        <source>Could not remove the scheduled task or the protected copy of ThroneCore.</source>
        <translation>无法删除计划任务或 ThroneCore 的受保护副本。</translation>
    </message>
    <message>
        <source>Could not prepare or start the updater.</source>
        <translation>无法准备或启动更新程序。</translation>
    </message>
    <message>
        <source>The helper failed with exit code 0x%1.</source>
        <translation>辅助程序失败，退出代码 0x%1。</translation>
    </message>
</context>
<context>
    <name>QGnomeTheme</name>
    <message>
        <source>&amp;OK</source>
        <translation>确定(&amp;O)</translation>
    </message>
    <message>
        <source>&amp;Save</source>
        <translation>保存(&amp;S)</translation>
    </message>
    <message>
        <source>&amp;Cancel</source>
        <translation>取消(&amp;C)</translation>
    </message>
    <message>
        <source>&amp;Close</source>
        <translation>关闭(&amp;C)</translation>
    </message>
    <message>
        <source>Close without Saving</source>
        <translation>关闭而不保存</translation>
    </message>
</context>
<context>
    <name>QLineEdit</name>
    <message>
        <source>&amp;Undo</source>
        <translation>撤销(&amp;U)</translation>
    </message>
    <message>
        <source>&amp;Redo</source>
        <translation>重做(&amp;R)</translation>
    </message>
    <message>
        <source>Cu&amp;t</source>
        <translation>剪切(&amp;T)</translation>
    </message>
    <message>
        <source>&amp;Copy</source>
        <translation>复制(&amp;C)</translation>
    </message>
    <message>
        <source>&amp;Paste</source>
        <translation>粘贴(&amp;P)</translation>
    </message>
    <message>
        <source>Delete</source>
        <translation>删除</translation>
    </message>
    <message>
        <source>Select All</source>
        <translation>全选</translation>
    </message>
</context>
<context>
    <name>QWidgetTextControl</name>
    <message>
        <source>&amp;Undo</source>
        <translation>撤销(&amp;U)</translation>
    </message>
    <message>
        <source>&amp;Redo</source>
        <translation>重做(&amp;R)</translation>
    </message>
    <message>
        <source>Cu&amp;t</source>
        <translation>剪切(&amp;T)</translation>
    </message>
    <message>
        <source>&amp;Copy</source>
        <translation>复制(&amp;C)</translation>
    </message>
    <message>
        <source>Copy &amp;Link Location</source>
        <translation>复制链接地址(&amp;L)</translation>
    </message>
    <message>
        <source>&amp;Paste</source>
        <translation>粘贴(&amp;P)</translation>
    </message>
    <message>
        <source>Delete</source>
        <translation>删除</translation>
    </message>
    <message>
        <source>Select All</source>
        <translation>全选</translation>
    </message>
</context>
</TS>
