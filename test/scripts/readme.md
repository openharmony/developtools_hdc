# pytest 测试框架 for hdc auto test

## 参数指导

参数可在pytest.ini中配置，也可以在命令行中配置，优先级：命令行 > pytest.ini
-s: 显示输出调试信息，包括print打印的信息，但是不会写入到report
-v: 显示更详细的信息
-n：支持多线程运行脚本（需要保持用例彼此独立）
--reruns NUM：失败用例重跑次数
-x：表示只要有一个用例报错，那么测试停止
-k：模糊匹配字符串进行用例跑测
-m: 标记用例Level执行级别

## 目录结构

目录结构如下：

```shell
~/hdc/test/scripts/
├─reports # 测试报告目录
├─resource # 测试资源文件，存放测试过程中使用到的文件，可使用prepare.py进行资源准备
├─testModule
|   ├─output # 命令行输出文件，例如:重定向文件，zip文件
|   ├─test_hdc_base.py # hdc基础测试
|   ├─test_hdc_file.py # hdc文件测试
|   └─utils.py # 工具
├─main.py # 测试用例执行入口
├─prepare.py # 资源准备脚本
├─pytest.ini # pytest配置文件
└─requirements.txt # 依赖文件
```

## 测试用例编写

### 1. 新建测试套（py文件）

建议文件名格式为：test_hdc_模块名.py

### 2. 编写测试用例

```python
import pytest
from utils import GP, check_version, load_gp # 注意这里load_gp不能缺省

class TestClassExample:
    test_table = ['A', 'B', 'C']
    @classmethod
    def setup_class(self):
        print("这里配置测试类执行前的操作")


    @classmethod
    def teardown_class(self):
        print("这里配置测试类执行后的操作")


    @pytest.mark.自定义标签 # 这里配置用例自定义标签，在pytest.ini中可配置自定义标签参数，以运行标记的测试用例
    @pytest.mark.repeat(2) # 这里配置用例重跑次数
    @check_version("Ver: 3.1.0a") # 这里配置用例最低可执行的版本，低于该版本的环境将跳过用例
    @pytest.mark.parametrize("test_item", file_table) # 这里配置参数化，支持列表，元组，字典
    def test_example(self, test_item):
        assert test_item in self.test_table # 这里编写测试用例,支持assert断言
```

## 测试资源配置

进入scripts目录，配置环境准备，执行以下命令，

```shell
python prepare.py
```

如已配置环境，执行以下命令，安装缺失的依赖：

```shell
python prepare.py -r
```

如已配置环境，执行以下命令，更新资源文件：

```shell
python prepare.py -s
```

如已配置设备，执行以下命令，更新设备信息：

```shell
python prepare.py -c
```

   > **说明：**
   >
   >1、新环境执行一次资源环境配置即可；
   >
   >2、配置过程存在需要确认的设备信息，请根据提示按下回车确认；
   >
   >3、如需生成软链接文件，需使用管理员权限执行资源配置命令；
   >
   >4、需要联网获取的资源文件，如存在无法下载的情况，请根据提示手动下载并解压到resource目录。

## 测试执行

配置完成环境后即可执行测试用例，执行用例有以下两种方式

### 方式一

```shell
python main.py
```

执行参数在pytest.main中配置

### 方式二

- 执行所有用例

```shell
pytest ./
```

- 执行指定目录下所有用例

```shell
pytest ./testModule/
```

- 执行指定测试文件

```shell
pytest ./testModule/test_hdc_base.py
```

- 执行指定测试用例类

```shell
pytest testModule/test_hdc_base.py::TestClassExample
```

- 执行指定测试用例

```shell
pytest testModule/test_hdc_base.py::TestClassExample::test_example
```

## 测试报告

执行python main.py后，会在reports目录下生成测试报告
