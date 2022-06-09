# mydocker
A simple implementation of docker engine, the final project of UCAS CloudComputing Course

## 运行
1. 进入`./mydocker`后，执行make 编译docker
2. 运行`./mydocker -h`显示提示信息
3. 典型的启动docker命令为`sudo ./docker -i /home/ubuntu/xh/centos.tar -r ./centos -m 25m -c 4000 -q 2000 -k`

## memory cgroup限制测试
启动docker之后，`cd resource-test &  ./mem-allocate`
mem-allocate是一个测试程序，它每隔一秒钟就申请分配1M的内存
在启动docker时，我们已经配置了cgroup使得docker中的进程占用内存不得超过一定的限制，我们可以看到mem-allocate程序会停止申请内存

## cpu cgroup限制测试
同memory cgroup测试类似，resource-test目录下有一个cpu-test程序，它就是一个空转的死循环，如果不加限制，它应该会几乎100%地占用CPU,我们已经通过配置cgroup限制了它的时间片，让它的占用不超过50%，运行`./cpu-test`，启动另一个shell，然后运行top命令，可以看到它的cpu占用会保持在50%以下，我们成功地限制了它的cpu时间

