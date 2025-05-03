document.addEventListener('DOMContentLoaded', function () {
  const botoesCarrinho = document.querySelectorAll('.btn-carrinho');
  const carrinho = JSON.parse(localStorage.getItem('carrinho')) || [];

  function salvarCarrinho() {
    localStorage.setItem('carrinho', JSON.stringify(carrinho));
    atualizarContadorCarrinho();
  }

  function atualizarContadorCarrinho() {
    const contadorCarrinho = document.querySelector('#contador-carrinho');
    if (contadorCarrinho) {
      contadorCarrinho.textContent = carrinho.length;
    }
  }

  botoesCarrinho.forEach(botao => {
    botao.addEventListener('click', () => {
      const item = botao.getAttribute('data-item');
      const preco = parseFloat(botao.getAttribute('data-preco'));

      carrinho.push({ item, preco });
      salvarCarrinho();
    });
  });

  const perguntas = document.querySelectorAll('.duvida h3');
  perguntas.forEach(pergunta => {
    pergunta.addEventListener('click', function () {
      const resposta = this.nextElementSibling;
      resposta.classList.toggle('visivel');
    });
  });

  const duvidas = document.querySelectorAll('.duvida');
  duvidas.forEach(duvida => {
    const pergunta = duvida.querySelector('h3');
    pergunta.addEventListener('click', () => {
      duvida.classList.toggle('ativa');
    });
  });

  if (document.querySelector('#lista-carrinho')) {
    const listaCarrinho = document.querySelector('#lista-carrinho');
    const subtotalEl = document.querySelector('#subtotal');
    const freteEl = document.querySelector('#frete');
    const totalEl = document.querySelector('#total');
    const botaoPagar = document.querySelector('#botao-pagar');

    function atualizarCarrinho() {
      let subtotal = 0;
      listaCarrinho.innerHTML = '';

      carrinho.forEach((produto, index) => {
        const itemEl = document.createElement('div');
        itemEl.classList.add('item-carrinho');
        itemEl.innerHTML = `
          <p>${produto.item} - R$ ${produto.preco.toFixed(2)}</p>
          <button class="btn-remover" data-index="${index}">Remover</button>
        `;
        listaCarrinho.appendChild(itemEl);
        subtotal += produto.preco;
      });

      const frete = 5.0;
      const total = subtotal + frete;

      subtotalEl.textContent = subtotal.toFixed(2);
      freteEl.textContent = frete.toFixed(2);
      totalEl.textContent = total.toFixed(2);

      const botoesRemover = document.querySelectorAll('.btn-remover');
      botoesRemover.forEach(botao => {
        botao.addEventListener('click', () => {
          const index = parseInt(botao.getAttribute('data-index'));
          carrinho.splice(index, 1);
          salvarCarrinho();
          atualizarCarrinho();
        });
      });
    }

    atualizarCarrinho();

    botaoPagar.addEventListener('click', () => {
      if (carrinho.length === 0) {
        alert('Seu carrinho está vazio!');
        return;
      }

      const subtotal = carrinho.reduce((acc, produto) => acc + produto.preco, 0);
      const frete = 5.0;
      const total = subtotal + frete;
      const horario = new Date().toLocaleString();

      // Envia todos os itens do carrinho
      const produtos = carrinho.map(produto => `${produto.item} (${produto.preco.toFixed(2)})`).join(', ');
      const body = `produtos=${encodeURIComponent(produtos)}&frete=${frete.toFixed(2)}`;

      fetch('/fazer-pedido', {
        method: 'POST',
        headers: {
          'Content-Type': 'application/x-www-form-urlencoded'
        },
        body: body
      })
      .then(response => response.text())
      .then(data => {
        alert('Pagamento realizado com sucesso! Pedido enviado para o servidor.');
        carrinho.length = 0; // Limpa o carrinho
        salvarCarrinho();
        atualizarCarrinho();
      })
      .catch(error => {
        console.error('Erro ao enviar o pedido:', error);
        alert('Erro ao processar o pedido.');
      });
    });
  }

  atualizarContadorCarrinho();
});