# -*- mode: ruby -*-
# vi: set ft=ruby :

Vagrant.configure(2) do |config|
  config.vm.box     = "ubuntu/trusty64"

  config.vm.provider "virtualbox" do |v|
    v.customize ["modifyvm", :id, "--memory", "2048", "--ioapic", "on"]
  end

  config.vm.synced_folder ".", "/vagrant"

  # Install build tools
  config.vm.provision "shell", inline: <<-SCRIPT
    apt-get update
    apt-get -y install build-essential autoconf
    apt-get -y autoremove
  SCRIPT

end
