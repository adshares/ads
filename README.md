<p align="center">
  <a href="https://adshares.net/">
    <img src="https://adshares.net/logos/ads.svg" alt="Adshares" width=100 height=100>
  </a>
  <h3 align="center"><small>ADS Blockchain</small></h3>
  <p align="center">
    <a href="https://github.com/adshares/ads/issues/new?template=bug_report.md&labels=Bug">Report bug</a>
    ·
    <a href="https://github.com/adshares/ads/issues/new?template=feature_request.md&labels=New%20Feature">Request feature</a>
    ·
    <a href="https://github.com/adshares/ads/wiki">Wiki</a>
  </p>
</p>

<br>

The ADS Blockchain is a fork of the Enterprise Service Chain ([ESC](https://github.com/EnterpriseServiceChain/esc)), a blockchain software technology capable of facilitating high volumes of simple transactions which, similarly to other cryptocurrencies, allows sending tokens between user accounts.
ESC derives its name from the concept of the Enterprise Service Bus, where a cryptocurrency is used as the communication protocol.

[![Quality Status](https://sonarcloud.io/api/project_badges/measure?project=adshares-ads&metric=alert_status)](https://sonarcloud.io/dashboard?id=adshares-ads)
[![Reliability Rating](https://sonarcloud.io/api/project_badges/measure?project=adshares-ads&metric=reliability_rating)](https://sonarcloud.io/dashboard?id=adshares-ads)
[![Security Rating](https://sonarcloud.io/api/project_badges/measure?project=adshares-ads&metric=security_rating)](https://sonarcloud.io/dashboard?id=adshares-ads)
[![Build Status](https://travis-ci.org/adshares/ads.svg?branch=master)](https://travis-ci.org/adshares/ads)


## Getting Started

### Master node

To setup the master node, you will need to provide a node number and secret key. 
Usually you will also need to specify a host name. 

1. Install binaries from official PPA

	```
	sudo add-apt-repository ppa:adshares/releases
	sudo apt-get update
	sudo apt-get install ads ads-tools
	```
    
2. Configure and start the node

	Configuration tool will ask you about the node number, a seckret key and a host name.
	 
	```
	ads-serivce configure
	ads-serivce start
	```

3. Check the node status

	```
	ads-monitor status -v
	```
	
	If that works, you should see current time, block time and node number.
	

### Connecting to the node

To connect to the node, you will usually need to provide an account address and secret key. 
If the node runs on a machine other than the one where you log in, you will also need to specify a host name. 
Once you know the proper parameters, you should be able to connect like this:

```
ads --host=HOST --address=ADDRESS -s
ENTER passphrase or secret key
****************************************************************
```

If that works, you should see some introductory information.
Then you can enter commands, for example, to get the current status of the user:
```
Working dir: /home/user/.ads
Public key: XYZ
Node port: 6511
Node host: 127.0.0.1
Address: 000X-0000000Y-AAAA

[...]

{"run":"get_me"}
``` 

See a list of all [available commands](https://github.com/adshares/ads/wiki/ADS-API#methods).

### Documentation

- [Installation](https://github.com/adshares/ads/wiki#installation)
- [Usage](https://github.com/adshares/ads/wiki/ADS-API)
- [QA tests](https://github.com/adshares/ads-tests)

### Contributing

Please follow our [Contributing Guidelines](docs/CONTRIBUTING.md)

### Versioning

We use [SemVer](http://semver.org/) for versioning. For the versions available, see the [tags on this repository](https://github.com/adshares/ads/tags). 


## Authors

- **[PicoStocks](https://github.com/picostocks)** - _architecture, programmer_
- **[Jacek Zemło](https://github.com/jzemlo)** - _architecture, programmer_

See also the list of [contributors](https://github.com/adshares/ads/contributors) who participated in this project.

### License

This work is licensed under the Creative Commons Attribution-NoDerivatives 4.0
 International License. To view a copy of this license, visit
 http://creativecommons.org/licenses/by-nd/4.0/ or send a letter to Creative
 Commons, PO Box 1866, Mountain View, CA 94042, USA.
  See the [LICENSE](LICENSE) file for details.


## More info

- [PHP ADS Client](https://github.com/adshares/ads-php-client)
- [ADS Tests](https://github.com/adshares/ads-tests)
- [Releases PPA](https://launchpad.net/~adshares/+archive/ubuntu/releases)
- [Snapshots PPA](https://launchpad.net/~adshares/+archive/ubuntu/snapshots)
