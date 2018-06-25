#!/bin/bash

# Expects the image_dir as the first argument
# Expects simulate_chkpt.sh to be inside image path
IMAGE_PATH=$1
IMAGE_DIR=$2
FLEXUS_PATH=$3
DOCKER_IMAGE=$4
UNAME=`whoami`
UID=`id -u`
GID=`id -g`
JID=`echo $(($(date +%s%N)/1000000))`


# Iterate over all QEMU checkpoint images, create .yaml file and run trace mode simulation
for chkpt in $IMAGE_PATH/$IMAGE_DIR/*/; do
        Chkpt=`basename "$chkpt"`
        echo "Launching job for checkpoint: " $Chkpt
        CID=`echo $(($(date +%s%N)/1000000))`
        cat > "$CID".yaml <<EOF
apiVersion: batch/v1                                                   
kind: Job                                                              
metadata:                                                              
  generateName: $UNAME-                                              
  labels:                                                              
    user: $UNAME                                                     
    chkpt: $Chkpt                                                          
    jid: "$JID"                                                  
spec:                                                                  
  backoffLimit: 3
  template:                                                            
    metadata:                                                          
      generateName: $UNAME-                                          
    spec:                                                              
      containers:                                                      
      - name: $UNAME-$Chkpt-$CID                                   
        image: $DOCKER_IMAGE                                            
        volumeMounts:                                                  
        - mountPath: /home/usr/qflex/images                            
          name: volume-images                                          
        - mountPath: /home/usr/qflex/flexus.so                            
          name: volume-flexus                                          
        imagePullPolicy: IfNotPresent                                  
        command: ["/usr/local/bin/entrypoint.sh"]                      
        args: ["/bin/bash", "/home/usr/qflex/images/simulate_chkpt.sh", "$IMAGE_DIR", "$Chkpt"]
        env:                                                           
        - name: LOCAL_USER_ID                                          
          value: "$UID"                                              
        - name: LOCAL_GROUP_ID                                         
          value: "$GID"                                               
      restartPolicy: OnFailure                                         
      volumes:                                                         
      - name: volume-images                                            
        hostPath:                                                      
         # directory location on host                                  
         path: $IMAGE_PATH                         
         # this field is optional                                      
         type: Directory                                               
      - name: volume-flexus                                            
        hostPath:                                                      
         # directory location on host                                  
         path: $FLEXUS_PATH                         
         # this field is optional                                      
         type: File                                            
EOF
        kubectl create -f $CID.yaml
        rm $CID.yaml
done
echo "Your job id is: " $JID
