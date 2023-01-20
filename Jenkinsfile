pipeline{
 agent any{
  stages{
   stage("build"){
   when{
    expression{
    //BRANCH_NAME is an environment variable, i.e., always available in Jenkinsfile
     BRANCH_NAME == 'master' || 'dev'
     //) && CODE_CHANGES == true
    }
   }
    steps{
     echo 'building the application ...'
    }
   }
   stage("test"){
   when{
    expression{
    //BRANCH_NAME is an environment variable, i.e., always available in Jenkinsfile
     BRANCH_NAME == 'test'
     //) && CODE_CHANGES == true
    }
   }
    steps{
     echo 'testing the application ...'
    }
   }
   stage("review"){
   when{
    expression{
    //BRANCH_NAME is an environment variable, i.e., always available in Jenkinsfile
     BRANCH_NAME == 'dev'
     //) && CODE_CHANGES == true
    }
   }
    steps{
     echo 'reviewing the application ...'
    }
   }
   stage("deploy"){
   when{
    expression{
    //BRANCH_NAME is an environment variable, i.e., always available in Jenkinsfile
     BRANCH_NAME == 'master'
     //) && CODE_CHANGES == true
    }
   }
    steps{
     echo 'deploying the application ...'
    }
   }
   post{
   //this will definitely execute like finally in java
    always{
     //specify the commands which need to be executed always
     //any kind of connection close statements
     echo 'I get executed always...'
    }
    success{
     //commands which need to be executed in the case when everything went well
     echo 'Build success'
    }
    failure{
     //commands to perform in case of a build failure
     //lets say you want to report the reason of failure
     echo 'Build failure'
    }
   }
  }
 }
}
