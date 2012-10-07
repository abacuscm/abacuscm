-- MySQL dump 10.10
--
-- Host: localhost    Database: abacus
-- ------------------------------------------------------
-- Server version	5.0.22-log

/*!40101 SET @OLD_CHARACTER_SET_CLIENT=@@CHARACTER_SET_CLIENT */;
/*!40101 SET @OLD_CHARACTER_SET_RESULTS=@@CHARACTER_SET_RESULTS */;
/*!40101 SET @OLD_COLLATION_CONNECTION=@@COLLATION_CONNECTION */;
/*!40101 SET NAMES utf8 */;
/*!40103 SET @OLD_TIME_ZONE=@@TIME_ZONE */;
/*!40103 SET TIME_ZONE='+00:00' */;
/*!40014 SET @OLD_UNIQUE_CHECKS=@@UNIQUE_CHECKS, UNIQUE_CHECKS=0 */;
/*!40014 SET @OLD_FOREIGN_KEY_CHECKS=@@FOREIGN_KEY_CHECKS, FOREIGN_KEY_CHECKS=0 */;
/*!40101 SET @OLD_SQL_MODE=@@SQL_MODE, SQL_MODE='NO_AUTO_VALUE_ON_ZERO' */;
/*!40111 SET @OLD_SQL_NOTES=@@SQL_NOTES, SQL_NOTES=0 */;

--
-- Table structure for table `Clarification`
--

DROP TABLE IF EXISTS `Clarification`;
CREATE TABLE `Clarification` (
  `clarification_id` int(11) NOT NULL default '0',
  `clarification_req_id` int(11) NOT NULL default '0',
  `user_id` int(11) NOT NULL default '0',
  `time` int(11) NOT NULL default '0',
  `public` tinyint(1) default NULL,
  `text` text CHARACTER SET utf8,
  PRIMARY KEY  (`clarification_id`),
  KEY `clarification_req_id` (`clarification_req_id`),
  KEY `user_id` (`user_id`)
--  CONSTRAINT `Clarification_ibfk_2` FOREIGN KEY (`clarification_req_id`) REFERENCES `ClarificationRequest` (`clarification_req_id`) ON UPDATE CASCADE,
--  CONSTRAINT `Clarification_ibfk_1` FOREIGN KEY (`user_id`) REFERENCES `User` (`user_id`) ON UPDATE CASCADE
) ENGINE=InnoDB DEFAULT CHARSET=latin1;

--
-- Table structure for table `ClarificationRequest`
--

DROP TABLE IF EXISTS `ClarificationRequest`;
CREATE TABLE `ClarificationRequest` (
  `clarification_req_id` int(11) NOT NULL default '0',
  `user_id` int(11) NOT NULL default '0',
  `time` int(11) NOT NULL default '0',
  `problem_id` int(11) default NULL,
  `text` text CHARACTER SET utf8,
  PRIMARY KEY  (`clarification_req_id`),
  KEY `user_id` (`user_id`),
  KEY `problem_id` (`problem_id`)
--  CONSTRAINT `ClarificationRequest_ibfk_1` FOREIGN KEY (`user_id`) REFERENCES `User` (`user_id`) ON UPDATE CASCADE
) ENGINE=InnoDB DEFAULT CHARSET=latin1;

--
-- Table structure for table `ContestStartStop`
--

DROP TABLE IF EXISTS `ContestStartStop`;
CREATE TABLE `ContestStartStop` (
  `server_id` int(10) unsigned NOT NULL default '0',
  `action` enum('START','STOP') NOT NULL default 'START',
  `time` int(10) unsigned NOT NULL default '0',
  PRIMARY KEY  (`server_id`,`action`,`time`)
) ENGINE=InnoDB DEFAULT CHARSET=latin1;

--
-- Table structure for table `PeerMessage`
--

DROP TABLE IF EXISTS `PeerMessage`;
CREATE TABLE `PeerMessage` (
  `server_id` int(10) unsigned NOT NULL default '0',
  `message_id` int(10) unsigned NOT NULL default '0',
  `message_type_id` int(10) unsigned NOT NULL default '0',
  `time` int(10) unsigned NOT NULL default '0',
  `signature` varchar(128) character set latin1 collate latin1_bin default NULL,
  `data` longblob,
  `processed` tinyint(1) default NULL,
  PRIMARY KEY  (`server_id`,`message_id`),
  KEY `processed` (`processed`),
  KEY `time` (`time`)
--  CONSTRAINT `PeerMessage_ibfk_1` FOREIGN KEY (`server_id`) REFERENCES `Server` (`server_id`) ON UPDATE CASCADE
) ENGINE=InnoDB DEFAULT CHARSET=latin1;

--
-- Table structure for table `PeerMessageNoAck`
--

DROP TABLE IF EXISTS `PeerMessageNoAck`;
CREATE TABLE `PeerMessageNoAck` (
  `server_id` int(10) unsigned NOT NULL default '0',
  `message_id` int(10) unsigned NOT NULL default '0',
  `ack_server_id` int(10) unsigned NOT NULL default '0',
  `lastsent` int(10) unsigned default NULL,
  PRIMARY KEY  (`server_id`,`message_id`,`ack_server_id`),
  KEY `lastsent` (`lastsent`),
  KEY `ack_server_id` (`ack_server_id`)
--  CONSTRAINT `PeerMessageNoAck_ibfk_3` FOREIGN KEY (`ack_server_id`) REFERENCES `Server` (`server_id`) ON UPDATE CASCADE,
--  CONSTRAINT `PeerMessageNoAck_ibfk_1` FOREIGN KEY (`server_id`) REFERENCES `Server` (`server_id`) ON UPDATE CASCADE,
--  CONSTRAINT `PeerMessageNoAck_ibfk_2` FOREIGN KEY (`server_id`, `message_id`) REFERENCES `PeerMessage` (`server_id`, `message_id`) ON UPDATE CASCADE
) ENGINE=InnoDB DEFAULT CHARSET=latin1;

--
-- Table structure for table `Problem`
--

DROP TABLE IF EXISTS `Problem`;
CREATE TABLE `Problem` (
  `problem_id` int(11) NOT NULL default '0',
  `updated` int(11) NOT NULL default '0',
  `type` varchar(16) default NULL,
  PRIMARY KEY  (`problem_id`)
) ENGINE=InnoDB DEFAULT CHARSET=latin1;

--
-- Table structure for table `ProblemAttributes`
--

DROP TABLE IF EXISTS `ProblemAttributes`;
CREATE TABLE `ProblemAttributes` (
  `problem_id` int(11) NOT NULL default '0',
  `attribute` varchar(128) NOT NULL default '',
  `value` varchar(255) default NULL,
  PRIMARY KEY  (`problem_id`,`attribute`)
--  CONSTRAINT `ProblemAttributes_ibfk_1` FOREIGN KEY (`problem_id`) REFERENCES `Problem` (`problem_id`) ON UPDATE CASCADE
) ENGINE=InnoDB DEFAULT CHARSET=latin1;

--
-- Table structure for table `ProblemDependencies`
--

DROP TABLE IF EXISTS `ProblemDependencies`;
CREATE TABLE `ProblemDependencies` (
  `problem_id` int(11) NOT NULL default '0',
  `dependent_problem_id` int(11) NOT NULL default '0',
  PRIMARY KEY (`problem_id`,`dependent_problem_id`)
--  CONSTRAINT `ProblemDependencies_ibfk_1` FOREIGN KEY (`problem_id`) REFERENCES `Problem` (`problem_id`) ON UPDATE CASCADE,
--  CONSTRAINT `ProblemDependencies_ibfk_2` FOREIGN KEY (`dependent_problem_id`) REFERENCES `Problem` (`problem_id`) ON UPDATE CASCADE
) ENGINE=InnoDB DEFAULT CHARSET=latin1;

--
-- Table structure for table `ProblemFileData`
--

DROP TABLE IF EXISTS `ProblemFileData`;
CREATE TABLE `ProblemFileData` (
  `problem_id` int(11) NOT NULL default '0',
  `attribute` varchar(128) NOT NULL default '',
  `data` longblob,
  PRIMARY KEY  (`problem_id`,`attribute`)
--  CONSTRAINT `ProblemFileData_ibfk_1` FOREIGN KEY (`problem_id`) REFERENCES `Problem` (`problem_id`) ON UPDATE CASCADE
) ENGINE=InnoDB DEFAULT CHARSET=latin1;

--
-- Table structure for table `Server`
--

DROP TABLE IF EXISTS `Server`;
CREATE TABLE `Server` (
  `server_id` int(10) unsigned NOT NULL default '0',
  `server_name` varchar(64) NOT NULL default '',
  PRIMARY KEY  (`server_id`),
  KEY `server_name` (`server_name`)
) ENGINE=InnoDB DEFAULT CHARSET=latin1;

--
-- Table structure for table `ServerAttributes`
--

DROP TABLE IF EXISTS `ServerAttributes`;
CREATE TABLE `ServerAttributes` (
  `server_id` int(10) unsigned NOT NULL default '0',
  `attribute` varchar(16) NOT NULL default '',
  `value` varchar(64) default NULL,
  PRIMARY KEY  (`server_id`,`attribute`)
--  CONSTRAINT `ServerAttributes_ibfk_1` FOREIGN KEY (`server_id`) REFERENCES `Server` (`server_id`) ON UPDATE CASCADE
) ENGINE=InnoDB DEFAULT CHARSET=latin1;

--
-- Table structure for table `Submission`
--

DROP TABLE IF EXISTS `Submission`;
CREATE TABLE `Submission` (
  `submission_id` int(11) NOT NULL default '0',
  `user_id` int(11) NOT NULL default '0',
  `prob_id` int(11) NOT NULL default '0',
  `time` int(11) NOT NULL default '0',
  `server_id` int(11) NOT NULL default '0',
  `content` longblob,
  `language` varchar(16) default NULL,
  PRIMARY KEY  (`submission_id`),
  KEY `user_id` (`user_id`),
  KEY `prob_id` (`prob_id`)
--  CONSTRAINT `Submission_ibfk_2` FOREIGN KEY (`prob_id`) REFERENCES `Problem` (`problem_id`) ON UPDATE CASCADE,
--  CONSTRAINT `Submission_ibfk_1` FOREIGN KEY (`user_id`) REFERENCES `User` (`user_id`) ON UPDATE CASCADE
) ENGINE=InnoDB DEFAULT CHARSET=latin1;

--
-- Table structure for table `SubmissionMark`
--

DROP TABLE IF EXISTS `SubmissionMark`;
CREATE TABLE `SubmissionMark` (
  `submission_id` int(11) NOT NULL default '0',
  `marker_id` int(11) NOT NULL default '0',
  `mark_time` int(11) NOT NULL default '0',
  `result` int(11) default NULL,
  `remark` varchar(255) default NULL,
  `server_id` int(11) default NULL,
  PRIMARY KEY  (`submission_id`,`marker_id`),
  KEY `marker_id` (`marker_id`)
--  CONSTRAINT `SubmissionMark_ibfk_2` FOREIGN KEY (`marker_id`) REFERENCES `User` (`user_id`) ON UPDATE CASCADE,
--  CONSTRAINT `SubmissionMark_ibfk_1` FOREIGN KEY (`submission_id`) REFERENCES `Submission` (`submission_id`) ON UPDATE CASCADE
) ENGINE=InnoDB DEFAULT CHARSET=latin1;

--
-- Table structure for table `SubmissionMarkFile`
--

DROP TABLE IF EXISTS `SubmissionMarkFile`;
CREATE TABLE `SubmissionMarkFile` (
  `submission_id` int(11) NOT NULL default '0',
  `marker_id` int(11) NOT NULL default '0',
  `name` varchar(255) NOT NULL default '',
  `content` longblob,
  PRIMARY KEY  (`submission_id`,`marker_id`,`name`),
  KEY `marker_id` (`marker_id`)
--  CONSTRAINT `SubmissionMarkFile_ibfk_2` FOREIGN KEY (`marker_id`) REFERENCES `User` (`user_id`) ON UPDATE CASCADE,
--  CONSTRAINT `SubmissionMarkFile_ibfk_1` FOREIGN KEY (`submission_id`) REFERENCES `Submission` (`submission_id`) ON UPDATE CASCADE
) ENGINE=InnoDB DEFAULT CHARSET=latin1;

--
-- Table structure for table `User`
--

DROP TABLE IF EXISTS `User`;
CREATE TABLE `User` (
  `user_id` int(11) NOT NULL default '0',
  `username` varchar(16) NOT NULL default '',
  `friendlyname` varchar(128) CHARACTER SET utf8 NOT NULL default '',
  `password` varchar(32) NOT NULL default '',
  `type` int(11) NOT NULL default '0',
  `group` int(11) NOT NULL default '1',
  PRIMARY KEY  (`user_id`),
  UNIQUE KEY `user_id` (`user_id`),
  UNIQUE KEY `username` (`username`),
  CONSTRAINT `User_ibfk_1` FOREIGN KEY (`group`) REFERENCES `Group` (`group_id`)
) ENGINE=InnoDB DEFAULT CHARSET=latin1;

--
-- Table structure for table `Group`
--

DROP TABLE IF EXISTS `Group`;
CREATE TABLE `Group` (
  `group_id` int(11) NOT NULL DEFAULT '0',
  `groupname` varchar(128) CHARACTER SET utf8 NOT NULL DEFAULT '',
  PRIMARY KEY (`group_id`),
  UNIQUE KEY `groupname` (`groupname`)
) ENGINE=InnoDB DEFAULT CHARSET=latin1;


/*!40103 SET TIME_ZONE=@OLD_TIME_ZONE */;

/*!40101 SET SQL_MODE=@OLD_SQL_MODE */;
/*!40014 SET FOREIGN_KEY_CHECKS=@OLD_FOREIGN_KEY_CHECKS */;
/*!40014 SET UNIQUE_CHECKS=@OLD_UNIQUE_CHECKS */;
/*!40101 SET CHARACTER_SET_CLIENT=@OLD_CHARACTER_SET_CLIENT */;
/*!40101 SET CHARACTER_SET_RESULTS=@OLD_CHARACTER_SET_RESULTS */;
/*!40101 SET COLLATION_CONNECTION=@OLD_COLLATION_CONNECTION */;
/*!40111 SET SQL_NOTES=@OLD_SQL_NOTES */;

