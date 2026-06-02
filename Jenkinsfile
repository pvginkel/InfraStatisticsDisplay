library('JenkinsPipelineUtils') _

withVault([vaultSecrets: [
    [path: 'kv/jenkins/iotsupport-pipeline-oidc', engineVersion: 2, secretValues: [
        [envVar: 'IOTSUPPORT_CLIENT_ID', vaultKey: 'client_id'],
        [envVar: 'IOTSUPPORT_CLIENT_SECRET', vaultKey: 'client_secret'],
    ]],
    [path: 'kv/shared/wifi-iot', engineVersion: 2, secretValues: [
        [envVar: 'WIFI_PASSWORD', vaultKey: 'password'],
    ]],
]]) {
    podTemplate(inheritFrom: 'jenkins-agent-large', containers: [
        containerTemplate(name: 'idf', image: 'espressif/idf:v5.5.3', command: 'sleep', args: 'infinity', envVars: [
            containerEnvVar(key: 'IOTSUPPORT_CLIENT_ID', value: '$IOTSUPPORT_CLIENT_ID'),
            containerEnvVar(key: 'IOTSUPPORT_CLIENT_SECRET', value: '$IOTSUPPORT_CLIENT_SECRET'),
            containerEnvVar(key: 'WIFI_PASSWORD', value: '$WIFI_PASSWORD'),
        ])
    ]) {
        node(POD_LABEL) {
            stage('Cloning repo') {
                dir('InfraStatisticsDisplay') {
                    checkout scm
                }

                dir('esp-libs') {
                     git branch: 'main',
                        credentialsId: '5f6fbd66-b41c-405f-b107-85ba6fd97f10',
                        url: 'https://github.com/pvginkel/esp-libs.git'
                }
            }

            stage('Build infra statistics display') {
                dir('InfraStatisticsDisplay') {
                    container('idf') {
                        // Necessary because the IDF container doesn't have support
                        // for setting the uid/gid.
                        sh 'git config --global --add safe.directory \'*\''
                        
                        sh '/opt/esp/entrypoint.sh scripts/dockerbuild.sh'
                    }
                }
            }
            
            stage('Deploy infra statistics display') {
                dir('InfraStatisticsDisplay') {
                    container('idf') {
                        sh 'chmod +x scripts/upload.sh'
                        sh 'scripts/upload.sh https://iot.ginbov.nl'
                    }
                }
            }
        }
    }
}
