const menuBtn = document.getElementById('menuBtn');
const nav = document.getElementById('nav');

if (menuBtn && nav) {
  menuBtn.addEventListener('click', () => {
    const isOpen = nav.classList.toggle('open');

    menuBtn.setAttribute('aria-expanded', String(isOpen));
  });

  document.querySelectorAll('.nav-links a').forEach((link) => {
    link.addEventListener('click', () => {
      nav.classList.remove('open');

      menuBtn.setAttribute('aria-expanded', 'false');
    });
  });
}

const carousel = document.querySelector('.carousel');

if (carousel) {
  const track = carousel.querySelector('.carousel-track');
  const prev = carousel.querySelector('[data-carousel-prev]');
  const next = carousel.querySelector('[data-carousel-next]');

  const scrollCarousel = (direction) => {
    const slide = track.querySelector('.carousel-slide');
    const gap = 18;
    const distance = slide ? slide.getBoundingClientRect().width + gap : track.clientWidth * 0.8;

    track.scrollBy({
      left: direction * distance,
      behavior: 'smooth'
    });
  };

  prev.addEventListener('click', () => scrollCarousel(-1));
  next.addEventListener('click', () => scrollCarousel(1));
}
