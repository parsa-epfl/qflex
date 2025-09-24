ARG BASE_IMAGE=parsa/qflex-release

FROM ${BASE_IMAGE}


WORKDIR /deps

RUN apt-get update && apt-get install -y curl build-essential
RUN curl https://sh.rustup.rs -sSf | sh -s -- -y
RUN . "$HOME/.cargo/env" 
RUN echo 'source $HOME/.cargo/env' >> /etc/bash.bashrc


WORKDIR /qflex

COPY ./WormCache /qflex/WormCache 

CMD ["bash"]



