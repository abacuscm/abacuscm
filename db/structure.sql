-- MySQL dump 9.11
--
-- Host: localhost    Database: abacus
-- ------------------------------------------------------
-- Server version	4.0.24

--
-- Table structure for table `PeerMessage`
--

CREATE TABLE PeerMessage (
  server_id int(10) unsigned NOT NULL default '0',
  seqnum int(10) unsigned NOT NULL default '0',
  signature varchar(128) binary default NULL,
  data blob,
  processed tinyint(1) default NULL,
  PRIMARY KEY  (server_id,seqnum),
  KEY processed (processed)
) TYPE=MyISAM;

--
-- Table structure for table `PeerMessageNoAck`
--

CREATE TABLE PeerMessageNoAck (
  server_id int(10) unsigned NOT NULL default '0',
  seqnum int(10) unsigned NOT NULL default '0',
  ack_server_id int(10) unsigned NOT NULL default '0',
  lastsent int(10) unsigned default NULL,
  PRIMARY KEY  (server_id,seqnum,ack_server_id),
  KEY lastsent (lastsent)
) TYPE=MyISAM;

--
-- Table structure for table `Server`
--

CREATE TABLE Server (
  server_id int(10) unsigned NOT NULL default '0',
  server_name varchar(64) NOT NULL default '',
  PRIMARY KEY  (server_id),
  KEY server_name (server_name)
) TYPE=MyISAM;

--
-- Table structure for table `ServerAttributes`
--

CREATE TABLE ServerAttributes (
  server_id int(10) unsigned NOT NULL default '0',
  attribute varchar(16) NOT NULL default '',
  value varchar(64) default NULL,
  PRIMARY KEY  (server_id,attribute)
) TYPE=MyISAM;

